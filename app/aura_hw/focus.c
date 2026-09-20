/****************************************************************************
 * aura_hw/focus.c
 * Aura-Focus 心流呼吸引导（对应方案：吸气 4s / 呼气 6s）
 *
 * 圆环随呼吸节奏缩放：吸气扩张、呼气收缩；阶段文案提示；
 * 默认 4 分钟一节，结束把心流时长计入五维 flow 维度。
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdio.h>
#include <unistd.h>

#include "util.h"
#include "radar_store.h"
#include "aura_app.h"

#define INHALE_MS       4000
#define EXHALE_MS       6000
#define SESSION_MS      (4 * 60 * 1000)

#define COLOR_BG      0x0b1020
#define COLOR_TEXT    0xffffff
#define COLOR_DIM     0x8890a8
#define COLOR_IN      0x48baf7
#define COLOR_OUT     0x55dfaa

typedef enum
{
  FC_IDLE = 0,
  FC_INHALE,
  FC_EXHALE,
  FC_DONE
} fc_state_t;

static fc_state_t g_state;
static uint32_t   g_phase_start;
static uint32_t   g_session_start;

static lv_obj_t *g_circle;
static lv_obj_t *g_lbl_phase;
static lv_obj_t *g_lbl_time;
static lv_obj_t *g_lbl_center;
static lv_obj_t *g_btn_start;

#define CIRCLE_MAX  130
#define CIRCLE_MIN  60

/****************************************************************************
 * 触屏控制
 ****************************************************************************/

static void on_start_click(lv_event_t *e)
{
  (void)e;

  if (g_state == FC_IDLE || g_state == FC_DONE)
    {
      g_state        = FC_INHALE;
      g_phase_start  = aura_millis();
      g_session_start = g_phase_start;
    }
}

/****************************************************************************
 * 界面
 ****************************************************************************/

static lv_obj_t *fc_make_label(lv_obj_t *parent, const char *text, int x,
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

static void focus_create(lv_obj_t *parent)
{
  /* 返回键由外壳统一添加（最高层级） */

  fc_make_label(parent, "Aura-Focus", 118, 12, &lv_font_montserrat_28,
                COLOR_TEXT);
  fc_make_label(parent, "inhale 4s / exhale 6s", 108, 44,
                &lv_font_montserrat_16, COLOR_DIM);

  g_circle = lv_obj_create(parent);
  lv_obj_set_size(g_circle, CIRCLE_MIN, CIRCLE_MIN);
  lv_obj_set_style_radius(g_circle, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(g_circle, lv_color_hex(COLOR_IN), 0);
  lv_obj_set_style_bg_opa(g_circle, LV_OPA_40, 0);
  lv_obj_set_style_border_width(g_circle, 3, 0);
  lv_obj_set_style_border_color(g_circle, lv_color_hex(COLOR_IN), 0);

  g_lbl_phase  = fc_make_label(parent, "", 140, 330,
                               &lv_font_montserrat_28, COLOR_TEXT);
  g_lbl_time   = fc_make_label(parent, "", 160, 300,
                               &lv_font_montserrat_20, COLOR_DIM);
  g_lbl_center = fc_make_label(parent, "4 min breathing session",
                               90, 200, &lv_font_montserrat_20,
                               COLOR_TEXT);

  g_btn_start = lv_obj_create(parent);
  lv_obj_set_size(g_btn_start, 140, 40);
  lv_obj_set_pos(g_btn_start, 125, 250);
  lv_obj_set_style_bg_color(g_btn_start, lv_color_hex(COLOR_IN), 0);
  lv_obj_set_style_bg_opa(g_btn_start, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(g_btn_start, 12, 0);
  lv_obj_set_style_border_width(g_btn_start, 0, 0);
  lv_obj_add_flag(g_btn_start, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(g_btn_start, on_start_click, LV_EVENT_CLICKED, NULL);
  {
    lv_obj_t *l = fc_make_label(g_btn_start, "START", 0, 0,
                                &lv_font_montserrat_20, COLOR_TEXT);
    lv_obj_center(l);   /* 手动坐标在此主题下有偏移，统一居中 */
  }

  g_state = FC_IDLE;
}

static void focus_delete(void)
{
  /* 中途退出：已进行的心流时间照计 */
  if (g_state == FC_INHALE || g_state == FC_EXHALE)
    {
      uint32_t mins = (aura_millis() - g_session_start) / 60000;

      if (mins > 0)
        {
          radar_store_apply_focus(mins);
        }
    }

  g_state = FC_IDLE;
}

static void focus_tick(uint32_t now)
{
  uint32_t elapsed;
  uint32_t remain;
  int      d;
  int      cx = 195;
  int      cy = 185;

  switch (g_state)
    {
      case FC_INHALE:
        elapsed = now - g_phase_start;
        if ((int32_t)(elapsed - INHALE_MS) >= 0)
          {
            g_state       = FC_EXHALE;
            g_phase_start = now;
            break;
          }

        d = CIRCLE_MIN +
            (int)((CIRCLE_MAX - CIRCLE_MIN) * elapsed / INHALE_MS);
        lv_obj_set_size(g_circle, d, d);
        lv_obj_set_pos(g_circle, cx - d / 2, cy - d / 2);
        lv_label_set_text(g_lbl_phase, "INHALE");
        lv_obj_set_style_text_color(g_lbl_phase,
                                    lv_color_hex(COLOR_IN), 0);
        break;

      case FC_EXHALE:
        elapsed = now - g_phase_start;
        if ((int32_t)(elapsed - EXHALE_MS) >= 0)
          {
            g_state       = FC_INHALE;
            g_phase_start = now;
            break;
          }

        d = CIRCLE_MAX -
            (int)((CIRCLE_MAX - CIRCLE_MIN) * elapsed / EXHALE_MS);
        lv_obj_set_size(g_circle, d, d);
        lv_obj_set_pos(g_circle, cx - d / 2, cy - d / 2);
        lv_label_set_text(g_lbl_phase, "EXHALE");
        lv_obj_set_style_text_color(g_lbl_phase,
                                    lv_color_hex(COLOR_OUT), 0);
        break;

      default:
        break;
    }

  if (g_state == FC_INHALE || g_state == FC_EXHALE)
    {
      remain = 0;
      if (now - g_session_start < SESSION_MS)
        {
          remain = (SESSION_MS - (now - g_session_start)) / 1000;
        }

      lv_label_set_text_fmt(g_lbl_time, "%u:%02u",
                            remain / 60, remain % 60);
      lv_obj_add_flag(g_btn_start, LV_OBJ_FLAG_HIDDEN);
      lv_label_set_text(g_lbl_center, "");

      if (remain == 0)
        {
          g_state = FC_DONE;
          radar_store_apply_focus(SESSION_MS / 60000);
        }
    }
  else if (g_state == FC_DONE)
    {
      lv_label_set_text(g_lbl_center, "DONE. flow recorded");
      lv_obj_remove_flag(g_btn_start, LV_OBJ_FLAG_HIDDEN);
      lv_label_set_text(g_lbl_phase, "");
      lv_label_set_text(g_lbl_time, "");
    }
  else
    {
      lv_label_set_text(g_lbl_phase, "");
      lv_label_set_text(g_lbl_time, "");
    }
}

const aura_module_t g_focus_module =
{
  .name   = "Aura-Focus",
  .create = focus_create,
  .delete = focus_delete,
  .tick   = focus_tick,
};
