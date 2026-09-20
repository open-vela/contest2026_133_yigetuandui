/****************************************************************************
 * aura_hw/coach.c
 * AI 数据教练看板：五维数据展示 + 本地规则引擎建议
 * （前端 focus-engine.js 的 C 移植，云端 AI 的降级路径——方案要求）
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdio.h>
#include <string.h>

#include "util.h"
#include "radar_store.h"
#include "aura_app.h"

#define COLOR_BG      0x0b1020
#define COLOR_TEXT    0xffffff
#define COLOR_DIM     0x8890a8
#define COLOR_BAR     0x48baf7
#define COLOR_BAR_BG  0x1a2238
#define COLOR_WEAK    0xffbf47
#define COLOR_STRONG  0x55dfaa

static lv_obj_t *g_bars[5];
static lv_obj_t *g_vals[5];
static lv_obj_t *g_lbl_score;
static lv_obj_t *g_lbl_suggest;

static const char   *g_dim_names[5] =
  { "Reaction", "Focus", "Memory", "Pressure", "Flow" };
static const uint8_t g_dim_weights[5] = { 22, 24, 22, 18, 14 };  /* balanced */

static const char *g_dim_actions[5] =
{
  "do a 3-min BeatFlicks warm-up; accuracy first.",
  "start a 4-min Aura-Focus session, then light training.",
  "run 2 sets of Synapse-N; raise N only above 75%.",
  "slow down: breathe 4 min before any training.",
  "shorter sessions, more breaks; breathe to re-enter flow."
};

/* pressure 语义与方案一致：数值越高越好（压力越低分越高由采集端保证，
 * 当前为占位维度，直接按普通维度参与加权）
 */
static void coach_analyze(const radar5_t *r, int *score, int *weakest,
                          int *strongest)
{
  int dims[5] = { r->reaction, r->focus, r->memory, r->pressure, r->flow };
  int total = 0;
  int wsum  = 0;
  int i;

  *weakest  = 0;
  *strongest = 0;

  for (i = 0; i < 5; i++)
    {
      total += dims[i] * g_dim_weights[i];
      wsum  += g_dim_weights[i];

      if (dims[i] < dims[*weakest])   *weakest  = i;
      if (dims[i] > dims[*strongest]) *strongest = i;
    }

  *score = total / wsum;
}

static lv_obj_t *co_make_label(lv_obj_t *parent, const char *text, int x,
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

static void coach_create(lv_obj_t *parent)
{
  radar5_t r;
  int      score;
  int      weakest;
  int      strongest;
  int      i;
  int      y;

  /* 返回键由外壳统一添加（最高层级） */

  co_make_label(parent, "AI Coach", 128, 12, &lv_font_montserrat_28,
                COLOR_TEXT);

  radar_store_get(&r);
  coach_analyze(&r, &score, &weakest, &strongest);

  g_lbl_score = NULL;
  {
    char buf[40];

    snprintf(buf, sizeof(buf), "Focus Score: %d", score);
    co_make_label(parent, buf, 108, 48, &lv_font_montserrat_20,
                  COLOR_TEXT);
  }

  /* 五维条形 */
  {
    int dims[5] = { r.reaction, r.focus, r.memory, r.pressure, r.flow };

    for (i = 0; i < 5; i++)
      {
        uint32_t name_color = COLOR_TEXT;

        y = 90 + i * 44;

        if (i == weakest)        name_color = COLOR_WEAK;
        else if (i == strongest) name_color = COLOR_STRONG;

        co_make_label(parent, g_dim_names[i], 30, y + 2,
                      &lv_font_montserrat_16, name_color);

        {
          lv_obj_t *bg = lv_obj_create(parent);
          lv_obj_t *fg;

          lv_obj_set_size(bg, 180, 16);
          lv_obj_set_pos(bg, 130, y + 4);
          lv_obj_set_style_bg_color(bg, lv_color_hex(COLOR_BAR_BG), 0);
          lv_obj_set_style_bg_opa(bg, LV_OPA_COVER, 0);
          lv_obj_set_style_radius(bg, 8, 0);
          lv_obj_set_style_border_width(bg, 0, 0);

          fg = lv_obj_create(bg);
          lv_obj_set_size(fg, dims[i] * 180 / 100, 16);
          lv_obj_set_pos(fg, 0, 0);
          lv_obj_set_style_bg_color(fg, lv_color_hex(COLOR_BAR), 0);
          lv_obj_set_style_bg_opa(fg, LV_OPA_COVER, 0);
          lv_obj_set_style_radius(fg, 8, 0);
          lv_obj_set_style_border_width(fg, 0, 0);
        }

        {
          char buf[8];

          snprintf(buf, sizeof(buf), "%d", dims[i]);
          g_vals[i] = co_make_label(parent, buf, 320, y + 2,
                                    &lv_font_montserrat_16, name_color);
        }

        g_bars[i] = NULL;   /* 静态页面，无需句柄 */
      }
  }

  /* 建议 */
  co_make_label(parent, "Suggestion", 30, 316,
                &lv_font_montserrat_16, COLOR_DIM);
  g_lbl_suggest = co_make_label(parent, "", 30, 340,
                                &lv_font_montserrat_16, COLOR_TEXT);
  lv_label_set_text_fmt(g_lbl_suggest, "%s is your focus:\n%s",
                        g_dim_names[weakest], g_dim_actions[weakest]);
  lv_obj_set_width(g_lbl_suggest, 330);
  lv_label_set_long_mode(g_lbl_suggest, LV_LABEL_LONG_WRAP);
}

static void coach_delete(void)
{
}

static void coach_tick(uint32_t now)
{
  (void)now;
}

const aura_module_t g_coach_module =
{
  .name   = "AI Coach",
  .create = coach_create,
  .delete = coach_delete,
  .tick   = coach_tick,
};
