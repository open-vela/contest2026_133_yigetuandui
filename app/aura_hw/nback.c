/****************************************************************************
 * aura_hw/nback.c
 * Synapse-N 记忆矩阵：Dual N-Back 引擎 + 2×2 四象限 LVGL 界面 + 触屏作答
 *
 * 规则（对应方案 3.1/3.2）：2×2 象限中色点以"淡入-保持-消失"节奏闪现，
 * 用户判断"当前色点的位置+颜色"是否与 N 步前一致：
 *   - 一致 → 在 2s 作答窗内点按 MATCH（触屏）
 *   - 不一致/超时未答 → 记为不答
 * 自适应 N 值：近 10 轮正确率 ≥75% 升 N（上限 4），≤50% 降 N（下限 1）。
 * 一局 30 轮，结束后把正确率写入五维 memory 维度。
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "util.h"
#include "event_bus.h"
#include "vib_motor.h"
#include "radar_store.h"
#include "aura_app.h"

#define NBACK_ROUNDS        30
#define STIM_SHOW_MS        850
#define ANSWER_WINDOW_MS    2000
#define STIM_INTERVAL_MS    2800     /* 相邻刺激间隔 */
#define ADAPT_WINDOW        10
#define ADAPT_UP_PCT        75
#define ADAPT_DOWN_PCT      50
#define N_MAX               4

#define COLOR_BG      0x0b1020
#define COLOR_TEXT    0xffffff
#define COLOR_DIM     0x8890a8
#define COLOR_RIGHT   0x55dfaa
#define COLOR_WRONG   0xff5b5b
#define COLOR_MATCH   0x48baf7

static const uint32_t g_stim_colors[3] = { 0xffbf47, 0x55dfaa, 0xc58cff };

/* ---- 引擎状态 ---- */
typedef enum
{
  NB_IDLE = 0,
  NB_SHOW,        /* 刺激显示中 */
  NB_WAIT,        /* 刺激消失，作答窗 */
  NB_FEEDBACK,    /* 正误反馈 */
  NB_DONE         /* 一局结束 */
} nb_state_t;

typedef struct
{
  uint8_t pos;    /* 0~3 象限 */
  uint8_t color;  /* 0~2 颜色 */
} nb_stim_t;

static nb_state_t  g_state;
static nb_stim_t   g_hist[NBACK_ROUNDS + N_MAX];
static int         g_round;             /* 当前轮次 0..29 */
static int         g_n;                 /* 当前 N 值 */
static int         g_score;
static int         g_answered;          /* 本轮是否已作答 */
static int         g_last_correct;      /* -1 无 0 错 1 对 */
static uint32_t    g_state_until;
static uint32_t    g_session_start;
static uint8_t     g_adapt_correct;     /* 最近 ADAPT_WINDOW 轮正确数 */
static uint8_t     g_adapt_count;
static uint32_t    g_lfsr = 0x1d2c3bu;

/* ---- LVGL 对象 ---- */
static lv_obj_t *g_quads[4];
static lv_obj_t *g_lbl_n;
static lv_obj_t *g_lbl_score;
static lv_obj_t *g_lbl_round;
static lv_obj_t *g_lbl_info;
static lv_obj_t *g_lbl_center;
static lv_obj_t *g_btn_match;
static lv_obj_t *g_btn_start;
static lv_obj_t *g_lbl_feedback;

static uint32_t nb_rand(void)
{
  g_lfsr = g_lfsr * 1664525u + 1013904223u;
  return g_lfsr >> 8;
}

/****************************************************************************
 * 引擎
 ****************************************************************************/

static void nb_new_stimulus(void)
{
  nb_stim_t *s = &g_hist[g_round];

  /* 25% 概率构造位置匹配，25% 概率构造颜色匹配（可控随机） */
  s->pos   = nb_rand() % 4;
  s->color = nb_rand() % 3;

  if (g_round >= g_n)
    {
      nb_stim_t *old = &g_hist[g_round - g_n];

      if ((nb_rand() % 100) < 25)
        {
          s->pos = old->pos;         /* 构造位置匹配 */
        }
      else if ((nb_rand() % 100) < 25)
        {
          s->color = old->color;     /* 构造颜色匹配 */
        }
    }

  g_state       = NB_SHOW;
  g_answered    = 0;
  g_last_correct = -1;
  g_state_until = aura_millis() + STIM_SHOW_MS;
}

static bool nb_is_match(void)
{
  nb_stim_t *cur;
  nb_stim_t *old;

  if (g_round < g_n)
    {
      return false;
    }

  cur = &g_hist[g_round];
  old = &g_hist[g_round - g_n];
  return cur->pos == old->pos && cur->color == old->color;
}

static void nb_judge(bool user_said_match)
{
  bool correct = (user_said_match == nb_is_match());

  if (correct)
    {
      g_score++;
      g_adapt_correct++;
    }

  g_adapt_count++;
  g_last_correct = correct ? 1 : 0;
  g_state        = NB_FEEDBACK;
  g_state_until  = aura_millis() + 500;

  /* 触觉编码（马达未接通时静默失败，不影响流程） */
  vib_request(correct ? VIB_PATTERN_DOUBLE_SHORT
                      : VIB_PATTERN_SINGLE_SHORT, VIB_PRIO_NBACK);
}

static void nb_adapt(void)
{
  if (g_adapt_count < ADAPT_WINDOW)
    {
      return;
    }

  if (g_adapt_correct * 100 >= ADAPT_UP_PCT * ADAPT_WINDOW &&
      g_n < N_MAX)
    {
      g_n++;
    }
  else if (g_adapt_correct * 100 <= ADAPT_DOWN_PCT * ADAPT_WINDOW &&
           g_n > 1)
    {
      g_n--;
    }

  g_adapt_correct = 0;
  g_adapt_count   = 0;
}

static void nb_start_session(void)
{
  g_round        = 0;
  g_n            = 2;
  g_score        = 0;
  g_adapt_correct = 0;
  g_adapt_count  = 0;
  g_session_start = aura_millis();

  nb_new_stimulus();
}

static void nb_finish(void)
{
  int acc = g_score * 100 / NBACK_ROUNDS;

  g_state = NB_DONE;
  radar_store_apply_nback((float)acc);
}

static void nb_engine_tick(uint32_t now)
{
  switch (g_state)
    {
      case NB_IDLE:
        break;

      case NB_SHOW:
        if ((int32_t)(now - g_state_until) >= 0)
          {
            g_state       = NB_WAIT;
            g_state_until = now + ANSWER_WINDOW_MS;
          }
        break;

      case NB_WAIT:
        if ((int32_t)(now - g_state_until) >= 0)
          {
            nb_judge(false);   /* 超时未答 = 判定"不一致"作答 */
          }
        break;

      case NB_FEEDBACK:
        if ((int32_t)(now - g_state_until) >= 0)
          {
            g_round++;
            nb_adapt();

            if (g_round >= NBACK_ROUNDS)
              {
                nb_finish();
              }
            else
              {
                nb_new_stimulus();
              }
          }
        break;

      case NB_DONE:
        break;

      default:
        break;
    }
}

/****************************************************************************
 * 触屏作答
 ****************************************************************************/

static void on_match_click(lv_event_t *e)
{
  (void)e;

  if (g_state == NB_SHOW || g_state == NB_WAIT)
    {
      if (!g_answered)
        {
          g_answered = 1;
          nb_judge(true);   /* 用户判定"一致" */
        }
    }
}

static void on_start_click(lv_event_t *e)
{
  (void)e;

  if (g_state == NB_IDLE || g_state == NB_DONE)
    {
      nb_start_session();
    }
}

/****************************************************************************
 * 界面
 ****************************************************************************/

static lv_obj_t *nb_make_label(lv_obj_t *parent, const char *text, int x,
                               int y, const lv_font_t *font,
                               uint32_t color)
{
  lv_obj_t *lbl = lv_label_create(parent);

  lv_label_set_text(lbl, text);
  lv_obj_set_style_text_font(lbl, font, 0);
  lv_obj_set_style_text_color(lbl, lv_color_hex(color), 0);
  lv_obj_set_pos(lbl, x, y);
  return lbl;
}

static void nback_create(lv_obj_t *parent)
{
  int i;

  /* 返回键由外壳统一添加（最高层级） */

  nb_make_label(parent, "Synapse-N", 118, 12, &lv_font_montserrat_28,
                COLOR_TEXT);
  nb_make_label(parent, "pos + color = N-back?", 96, 44,
                &lv_font_montserrat_16, COLOR_DIM);

  g_lbl_n     = nb_make_label(parent, "N=2", 30, 76,
                              &lv_font_montserrat_20, COLOR_MATCH);
  g_lbl_score = nb_make_label(parent, "Score 0", 268, 76,
                              &lv_font_montserrat_20, COLOR_TEXT);
  g_lbl_round = nb_make_label(parent, "Round 0/30", 30, 100,
                              &lv_font_montserrat_16, COLOR_DIM);

  /* 2×2 象限（190x140 区域，四块 88x62） */
  for (i = 0; i < 4; i++)
    {
      int x = 53 + (i % 2) * 100;
      int y = 130 + (i / 2) * 74;

      g_quads[i] = lv_obj_create(parent);
      lv_obj_set_size(g_quads[i], 88, 62);
      lv_obj_set_pos(g_quads[i], x, y);
      lv_obj_set_style_bg_color(g_quads[i], lv_color_hex(0x1a2238), 0);
      lv_obj_set_style_bg_opa(g_quads[i], LV_OPA_COVER, 0);
      lv_obj_set_style_radius(g_quads[i], 10, 0);
      lv_obj_set_style_border_width(g_quads[i], 1, 0);
      lv_obj_set_style_border_color(g_quads[i], lv_color_hex(COLOR_DIM),
                                    0);
    }

  g_lbl_feedback = nb_make_label(parent, "", 150, 296,
                                 &lv_font_montserrat_28, COLOR_RIGHT);

  g_lbl_info = nb_make_label(parent,
                             "tap MATCH when pos+color repeats",
                             40, 330, &lv_font_montserrat_16, COLOR_DIM);

  /* MATCH 作答按钮（上移避让底部返回键） */
  g_btn_match = lv_obj_create(parent);
  lv_obj_set_size(g_btn_match, 220, 40);
  lv_obj_set_pos(g_btn_match, 85, 346);
  lv_obj_set_style_bg_color(g_btn_match, lv_color_hex(COLOR_MATCH), 0);
  lv_obj_set_style_bg_opa(g_btn_match, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(g_btn_match, 12, 0);
  lv_obj_set_style_border_width(g_btn_match, 0, 0);
  lv_obj_add_flag(g_btn_match, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(g_btn_match, on_match_click, LV_EVENT_CLICKED, NULL);
  {
    lv_obj_t *l = nb_make_label(g_btn_match, "MATCH", 0, 0,
                                &lv_font_montserrat_20, COLOR_TEXT);
    lv_obj_center(l);
  }

  /* 居中的 START / 结果提示 */
  g_lbl_center = nb_make_label(parent, "", 60, 200,
                               &lv_font_montserrat_20, COLOR_TEXT);

  g_btn_start = lv_obj_create(parent);
  lv_obj_set_size(g_btn_start, 140, 40);
  lv_obj_set_pos(g_btn_start, 125, 250);
  lv_obj_set_style_bg_color(g_btn_start, lv_color_hex(COLOR_RIGHT), 0);
  lv_obj_set_style_bg_opa(g_btn_start, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(g_btn_start, 12, 0);
  lv_obj_set_style_border_width(g_btn_start, 0, 0);
  lv_obj_add_flag(g_btn_start, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(g_btn_start, on_start_click, LV_EVENT_CLICKED, NULL);
  {
    lv_obj_t *l = nb_make_label(g_btn_start, "START", 0, 0,
                                &lv_font_montserrat_20, COLOR_TEXT);
    lv_obj_center(l);
  }

  g_state = NB_IDLE;
  g_n     = 2;   /* 初始 N 值（开局前也应显示正确） */
}

static void nback_delete(void)
{
  g_state = NB_IDLE;
}

static void nback_tick(uint32_t now)
{
  int i;

  nb_engine_tick(now);

  /* 象限显示：刺激显示期点亮对应象限颜色，其余熄灭 */
  for (i = 0; i < 4; i++)
    {
      uint32_t bg = 0x1a2238;

      if (g_state == NB_SHOW && i == g_hist[g_round].pos)
        {
          bg = g_stim_colors[g_hist[g_round].color];
        }

      lv_obj_set_style_bg_color(g_quads[i], lv_color_hex(bg), 0);
    }

  lv_label_set_text_fmt(g_lbl_n, "N=%d", g_n);
  lv_label_set_text_fmt(g_lbl_score, "Score %d", g_score);
  lv_label_set_text_fmt(g_lbl_round, "Round %d/30", g_round);

  /* 反馈与状态文案 */
  if (g_state == NB_FEEDBACK && g_last_correct >= 0)
    {
      lv_label_set_text(g_lbl_feedback,
                        g_last_correct ? "OK" : "X");
      lv_obj_set_style_text_color(g_lbl_feedback,
                                  lv_color_hex(g_last_correct
                                               ? COLOR_RIGHT
                                               : COLOR_WRONG), 0);
    }
  else
    {
      lv_label_set_text(g_lbl_feedback, "");
    }

  if (g_state == NB_IDLE)
    {
      lv_label_set_text(g_lbl_center,
                        "Dual N-Back\nremember pos + color");
      lv_obj_remove_flag(g_btn_start, LV_OBJ_FLAG_HIDDEN);
    }
  else if (g_state == NB_DONE)
    {
      lv_label_set_text_fmt(g_lbl_center,
                            "DONE  Score %d/30  ACC %d%%",
                            g_score, g_score * 100 / NBACK_ROUNDS);
      lv_obj_remove_flag(g_btn_start, LV_OBJ_FLAG_HIDDEN);
    }
  else
    {
      lv_label_set_text(g_lbl_center, "");
      lv_obj_add_flag(g_btn_start, LV_OBJ_FLAG_HIDDEN);
    }
}

const aura_module_t g_nback_module =
{
  .name   = "Synapse-N",
  .create = nback_create,
  .delete = nback_delete,
  .tick   = nback_tick,
};
