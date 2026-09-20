/****************************************************************************
 * aura_hw/aura_ui.c
 * BeatFlicks 模块（LVGL 界面 + 手势/触屏双输入 + 音乐）
 *
 * 输入：IMU 甩腕手势（gesture_recognizer 经 EventBus）+
 *       触屏点按三条轨道（点击直接发布同类型手势事件，走同一判定路径）
 * 音乐：music.c 节拍音轨；/dev/audio 缺失时降级为边框节拍脉冲
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "util.h"
#include "event_bus.h"
#include "gesture_recognizer.h"
#include "game_port.h"
#include "music.h"
#include "aura_app.h"
#include "aura_ui.h"

#include "game_engine.h"

/* 与 game_port.c 内置演示谱面保持一致（改动需同步） */
static const char *s_ui_beatmap =
  "{ \"bpm\": 120, \"notes\": ["
  "{\"timestamp_ms\": 2000, \"type\": 0, \"track_id\": 1},"
  "{\"timestamp_ms\": 3000, \"type\": 1, \"track_id\": 1},"
  "{\"timestamp_ms\": 4000, \"type\": 2, \"track_id\": 1},"
  "{\"timestamp_ms\": 5000, \"type\": 0, \"track_id\": 1},"
  "{\"timestamp_ms\": 6000, \"type\": 2, \"track_id\": 1},"
  "{\"timestamp_ms\": 7000, \"type\": 1, \"track_id\": 1},"
  "{\"timestamp_ms\": 8000, \"type\": 0, \"track_id\": 1},"
  "{\"timestamp_ms\": 9000, \"type\": 2, \"track_id\": 1}"
  "]}";

#define LANE_COUNT      3
#define TARGET_Y        356
#define APPROACH_MS     1500
#define MISS_LINGER_MS  150
#define NOTE_W          64
#define NOTE_H          20
#define FLASH_MS        450
#define HIT_SOUND_MS    25

#define COLOR_BG        0x0b1020
#define COLOR_LEFT      0x55dfaa
#define COLOR_UP        0xffbf47
#define COLOR_RIGHT     0x48baf7
#define COLOR_TEXT      0xffffff
#define COLOR_DIM       0x8890a8
#define COLOR_PERFECT   0xffd34d
#define COLOR_GOOD      0x55dfaa
#define COLOR_MISS      0xff5b5b

static const int      g_lane_x[LANE_COUNT]   = { 65, 195, 325 };
static const uint32_t g_lane_color[LANE_COUNT] =
  { COLOR_UP, COLOR_LEFT, COLOR_RIGHT };
static const char    *g_lane_key[LANE_COUNT]  = { "U", "L", "R" };

/* ---- 跨线程共享状态 ---- */
static volatile uint32_t g_round_start;
static volatile int      g_score;
static volatile int      g_combo;
static volatile int      g_flash;
static volatile uint32_t g_flash_until;
static volatile int      g_round_done;
static volatile char     g_grade;
static volatile float    g_acc;

#define MAX_NOTES 32
static volatile uint8_t  g_judged[MAX_NOTES];
static volatile uint8_t  g_sounded[MAX_NOTES];

/* ---- LVGL 对象 ---- */
static lv_obj_t *g_lbl_score;
static lv_obj_t *g_lbl_combo;
static lv_obj_t *g_lbl_acc;
static lv_obj_t *g_lbl_flash;
static lv_obj_t *g_lbl_round;
static lv_obj_t *g_pulse;
static lv_obj_t *g_notes[MAX_NOTES];

static Beatmap  *g_map;
static uint32_t  g_pulse_until;

/****************************************************************************
 * 触屏输入：点按轨道 = 发布同类手势事件（走同一判定路径）
 ****************************************************************************/

static void on_lane_click(lv_event_t *e)
{
  int              lane = (int)(intptr_t)lv_event_get_user_data(e);
  gesture_event_t  ev;

  printf("[ui] lane tap %d\n", lane);

  ev.type         = lane;             /* 轨道与 NoteType 同序 */
  ev.timestamp_ms = aura_millis();
  ev.confidence   = 1.0f;             /* 触屏无置信度问题 */

  aura_event_publish(AURA_EVENT_GESTURE, &ev);
}

/****************************************************************************
 * EventBus 回调（非 UI 线程，仅写 volatile 变量与音效队列）
 ****************************************************************************/

static void on_round_start(aura_event_type_t type, const void *payload,
                           void *user)
{
  int i;

  (void)type;
  (void)user;

  g_round_start = *(const uint32_t *)payload;
  g_score       = 0;
  g_combo       = 0;
  g_flash       = 0;
  g_round_done  = 0;

  for (i = 0; i < MAX_NOTES; i++)
    {
      g_judged[i]  = 0;
      g_sounded[i] = 0;
    }
}

static void on_judge(aura_event_type_t type, const void *payload,
                     void *user)
{
  const aura_judge_info_t *info = payload;

  (void)type;
  (void)user;

  if (info->note_index >= 0 && info->note_index < MAX_NOTES)
    {
      g_judged[info->note_index] = 1;
    }

  switch (info->result)
    {
      case JUDGE_PERFECT:
        g_score += 10;
        g_combo += 1;
        g_flash = 1;
        break;
      case JUDGE_GOOD:
        g_score += 5;
        g_combo += 1;
        g_flash = 2;
        break;
      case JUDGE_MISS:
        g_combo = 0;
        g_flash = 3;
        break;
      default:
        break;
    }

  g_flash_until = aura_millis() + FLASH_MS;
  music_judge(info->result);
}

static void on_result(aura_event_type_t type, const void *payload,
                      void *user)
{
  const GameResultEvent *r = payload;

  (void)type;
  (void)user;

  g_grade      = r->grade;
  g_acc        = r->accuracy;
  g_round_done = 1;
}

void aura_ui_init_events(void)
{
  g_round_start = aura_millis();
  aura_event_subscribe(AURA_EVENT_ROUND_START, on_round_start, NULL);
  aura_event_subscribe(AURA_EVENT_JUDGE, on_judge, NULL);
  aura_event_subscribe(AURA_EVENT_GAME_RESULT, on_result, NULL);
}

/****************************************************************************
 * 界面构建
 ****************************************************************************/

static lv_obj_t *make_label(lv_obj_t *parent, const char *text, int x,
                            int y, const lv_font_t *font, uint32_t color)
{
  lv_obj_t *lbl = lv_label_create(parent);

  lv_label_set_text(lbl, text);
  lv_obj_set_style_text_font(lbl, font, 0);
  lv_obj_set_style_text_color(lbl, lv_color_hex(color), 0);
  lv_obj_set_pos(lbl, x, y);
  return lbl;
}

static lv_obj_t *make_box(lv_obj_t *parent, int x, int y, int w, int h,
                          uint32_t color, int radius)
{
  lv_obj_t *obj = lv_obj_create(parent);

  lv_obj_set_size(obj, w, h);
  lv_obj_set_pos(obj, x, y);
  lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(obj, radius, 0);
  lv_obj_set_style_border_width(obj, 0, 0);
  return obj;
}

static void build_screen(lv_obj_t *parent)
{
  int i;

  make_label(parent, "BeatFlicks", 118, 12, &lv_font_montserrat_28,
             COLOR_TEXT);
  make_label(parent, "flick or tap", 130, 44, &lv_font_montserrat_16,
             COLOR_DIM);

  make_label(parent, "SCORE", 30, 84, &lv_font_montserrat_16, COLOR_DIM);
  g_lbl_score = make_label(parent, "0", 30, 104, &lv_font_montserrat_28,
                           COLOR_TEXT);

  make_label(parent, "COMBO", 268, 84, &lv_font_montserrat_16, COLOR_DIM);
  g_lbl_combo = make_label(parent, "0", 268, 104, &lv_font_montserrat_28,
                           COLOR_TEXT);

  g_lbl_acc = make_label(parent, "ACC 0%", 150, 116,
                         &lv_font_montserrat_16, COLOR_DIM);

  g_lbl_flash = make_label(parent, "", 90, 190, &lv_font_montserrat_48,
                           COLOR_PERFECT);
  lv_obj_add_flag(g_lbl_flash, LV_OBJ_FLAG_HIDDEN);

  g_lbl_round = make_label(parent, "", 60, 220, &lv_font_montserrat_20,
                           COLOR_TEXT);
  lv_obj_add_flag(g_lbl_round, LV_OBJ_FLAG_HIDDEN);

  /* 节拍脉冲边框（音频缺失时的视觉降级） */
  g_pulse = make_box(parent, 0, 0, 390, 450, COLOR_UP, 0);
  lv_obj_set_style_bg_opa(g_pulse, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(g_pulse, 6, 0);
  lv_obj_set_style_border_color(g_pulse, lv_color_hex(COLOR_UP), 0);
  lv_obj_set_style_border_opa(g_pulse, LV_OPA_TRANSP, 0);
  lv_obj_add_flag(g_pulse, LV_OBJ_FLAG_IGNORE_LAYOUT);

  /* 三条轨道目标（可点按 = 触屏输入） */
  for (i = 0; i < LANE_COUNT; i++)
    {
      lv_obj_t *t = make_box(parent, g_lane_x[i] - 40, TARGET_Y, 80, 30,
                             g_lane_color[i], 8);
      lv_obj_set_style_bg_opa(t, LV_OPA_40, 0);
      lv_obj_add_flag(t, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_add_event_cb(t, on_lane_click, LV_EVENT_CLICKED,
                          (void *)(intptr_t)i);
      make_label(t, g_lane_key[i], 33, 5, &lv_font_montserrat_20,
                 COLOR_TEXT);
    }

  for (i = 0; i < MAX_NOTES; i++)
    {
      g_notes[i] = make_box(parent, 0, 0, NOTE_W, NOTE_H, 0xffffff,
                            NOTE_H / 2);
      lv_obj_add_flag(g_notes[i], LV_OBJ_FLAG_HIDDEN);
    }
}

/****************************************************************************
 * 每帧刷新
 ****************************************************************************/

static void update_notes(uint32_t now)
{
  uint32_t elapsed = now - g_round_start;
  int      i;

  if (g_map == NULL)
    {
      return;
    }

  for (i = 0; i < (int)g_map->total_notes && i < MAX_NOTES; i++)
    {
      int32_t rel = (int32_t)g_map->notes[i].timestamp_ms -
                    (int32_t)elapsed;
      int     type = (int)g_map->notes[i].type;

      /* 音符到达判定线：触发重音（一次） */
      if (!g_sounded[i] && rel <= HIT_SOUND_MS && rel > -MISS_LINGER_MS)
        {
          g_sounded[i] = 1;
          music_note_hit(type);
        }

      if (!g_judged[i] && rel < APPROACH_MS && rel > -MISS_LINGER_MS &&
          type >= 0 && type < LANE_COUNT)
        {
          int y = TARGET_Y - (rel * (TARGET_Y + 40)) / APPROACH_MS;

          lv_obj_set_pos(g_notes[i], g_lane_x[type] - NOTE_W / 2, y);
          lv_obj_set_style_bg_color(g_notes[i],
                                    lv_color_hex(g_lane_color[type]), 0);
          lv_obj_remove_flag(g_notes[i], LV_OBJ_FLAG_HIDDEN);
        }
      else
        {
          lv_obj_add_flag(g_notes[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void update_hud(uint32_t now)
{
  static const char *flash_text[]  = { "", "Perfect", "Good", "Miss" };
  static const uint32_t flash_color[] =
    { COLOR_TEXT, COLOR_PERFECT, COLOR_GOOD, COLOR_MISS };
  int judged_count;
  int i;

  lv_label_set_text_fmt(g_lbl_score, "%d", g_score);
  lv_label_set_text_fmt(g_lbl_combo, "%d", g_combo);

  if (g_flash > 0 && now < g_flash_until)
    {
      lv_label_set_text(g_lbl_flash, flash_text[g_flash]);
      lv_obj_set_style_text_color(g_lbl_flash,
                                  lv_color_hex(flash_color[g_flash]), 0);
      lv_obj_remove_flag(g_lbl_flash, LV_OBJ_FLAG_HIDDEN);
    }
  else
    {
      lv_obj_add_flag(g_lbl_flash, LV_OBJ_FLAG_HIDDEN);
    }

  judged_count = 0;
  for (i = 0; i < MAX_NOTES; i++)
    {
      judged_count += g_judged[i];
    }

  if (judged_count > 0)
    {
      int max_score = judged_count * 10;
      lv_label_set_text_fmt(g_lbl_acc, "ACC %d%%",
                            g_score * 100 / (max_score > 0 ? max_score : 1));
    }

  if (g_round_done)
    {
      lv_label_set_text_fmt(g_lbl_round, "GRADE %c  ACC %d%%",
                            g_grade, (int)(g_acc * 100.0f));
      lv_obj_remove_flag(g_lbl_round, LV_OBJ_FLAG_HIDDEN);
    }
  else
    {
      lv_obj_add_flag(g_lbl_round, LV_OBJ_FLAG_HIDDEN);
    }
}

/* 音频缺失时的节拍边框脉冲（仅在状态翻转时改样式，
 * 避免每帧全屏无效化导致渲染占满 UI 线程）
 */
static void update_pulse(uint32_t now)
{
  static bool pulse_on;

  if (music_audio_ok())
    {
      return;
    }

  if (music_consume_beat_pulse() > 0)
    {
      g_pulse_until = now + 90;
    }

  if (now < g_pulse_until && !pulse_on)
    {
      pulse_on = true;
      lv_obj_set_style_border_opa(g_pulse, LV_OPA_60, 0);
    }
  else if (now >= g_pulse_until && pulse_on)
    {
      pulse_on = false;
      lv_obj_set_style_border_opa(g_pulse, LV_OPA_TRANSP, 0);
    }
}

/****************************************************************************
 * 模块接口
 ****************************************************************************/

static void beatflicks_create(lv_obj_t *parent)
{
  /* 注意：返回键由外壳在 create 之后统一添加（保证最高层级），
   * 模块内不要再 aura_app_add_back，否则会被 g_pulse 全屏层盖住。
   */
  build_screen(parent);

  if (g_map == NULL)
    {
      g_map = ParseBeatmapJSON(s_ui_beatmap);
    }

  music_start(g_map ? g_map->bpm : 120);

  if (game_port_start(NULL) < 0)
    {
      printf("[ui] game_port_start failed\n");
    }
}

static void beatflicks_delete(void)
{
  game_port_stop();
  music_stop();
}

static void beatflicks_tick(uint32_t now)
{
  update_notes(now);
  update_hud(now);
  update_pulse(now);
  music_tick(now, g_round_start);
}

const aura_module_t g_beatflicks_module =
{
  .name   = "BeatFlicks",
  .create = beatflicks_create,
  .delete = beatflicks_delete,
  .tick   = beatflicks_tick,
};
