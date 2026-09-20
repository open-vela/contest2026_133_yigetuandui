/****************************************************************************
 * aura_hw/aura_app.c
 * Aura-Space 手表应用外壳
 *
 * 主屏：Aura-Space 标题 + 四个模块入口（BeatFlicks / Synapse-N /
 * Aura-Focus / Coach），触屏点击进入；每个模块屏幕左上角有返回键。
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdio.h>
#include <unistd.h>

#include "util.h"
#include "aura_app.h"

#define COLOR_BG     0x0b1020
#define COLOR_TEXT   0xffffff
#define COLOR_DIM    0x8890a8
#define COLOR_BTN1   0xffbf47
#define COLOR_BTN2   0x55dfaa
#define COLOR_BTN3   0x48baf7
#define COLOR_BTN4   0xc58cff

static lv_obj_t          *g_home;
static lv_obj_t          *g_module_scr;
static const aura_module_t *g_active;

static const aura_module_t *g_modules[4];
static const uint32_t      g_mod_colors[4] =
  { COLOR_BTN1, COLOR_BTN2, COLOR_BTN3, COLOR_BTN4 };

static void on_bg_touch(lv_event_t *e);
static void on_screen_swipe(lv_event_t *e);

/****************************************************************************
 * 导航
 ****************************************************************************/

static void close_module(void)
{
  printf("[app] close: delete module\n");

  if (g_active && g_active->delete)
    {
      g_active->delete();
    }

  printf("[app] close: delete screen\n");
  g_active = NULL;

  if (g_module_scr)
    {
      lv_obj_delete(g_module_scr);
      g_module_scr = NULL;
    }

  if (g_home)
    {
      lv_obj_remove_flag(g_home, LV_OBJ_FLAG_HIDDEN);
    }

  printf("[app] close: home shown\n");
}

static void open_module(const aura_module_t *m)
{
  printf("[app] open module: %s\n", m->name);
  lv_obj_add_flag(g_home, LV_OBJ_FLAG_HIDDEN);

  g_module_scr = lv_obj_create(lv_screen_active());
  lv_obj_set_size(g_module_scr, 390, 450);
  lv_obj_set_pos(g_module_scr, 0, 0);
  lv_obj_set_style_bg_color(g_module_scr, lv_color_hex(COLOR_BG), 0);
  lv_obj_set_style_border_width(g_module_scr, 0, 0);
  lv_obj_set_style_radius(g_module_scr, 0, 0);
  lv_obj_add_flag(g_module_scr, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(g_module_scr, on_bg_touch, LV_EVENT_CLICKED, NULL);

  g_active = m;
  if (m->create)
    {
      m->create(g_module_scr);
    }

  /* 右滑返回：PRESSED/RELEASED 经事件冒泡捕获，整屏任意位置生效，
   * 不受模块内装饰层遮挡影响。
   */
  lv_obj_add_event_cb(g_module_scr, on_screen_swipe, LV_EVENT_PRESSED, NULL);
  lv_obj_add_event_cb(g_module_scr, on_screen_swipe, LV_EVENT_RELEASED, NULL);

  /* 底部提示（纯装饰） */
  {
    lv_obj_t *hint = lv_label_create(g_module_scr);
    lv_label_set_text(hint, "swipe right = home");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(COLOR_DIM), 0);
    lv_obj_set_pos(hint, 108, 420);
  }
}

static void on_module_click(lv_event_t *e)
{
  int idx = (int)(intptr_t)lv_event_get_user_data(e);

  open_module(g_modules[idx]);
}

/* 诊断探针：任何落在模块屏幕上的点击（经事件冒泡）都打印坐标，
 * 用于区分"LVGL 完全看不到触摸"与"路由不到具体按钮"。
 */
static void on_bg_touch(lv_event_t *e)
{
  lv_indev_t *indev = lv_indev_active();
  lv_point_t  p;

  if (indev)
    {
      lv_indev_get_point(indev, &p);
      printf("[touch] x=%d y=%d\n", (int)p.x, (int)p.y);
    }
}

/* ---- 右滑返回：整屏监听按下/松开（事件冒泡，子控件上的滑动同样生效） ---- */
#define SWIPE_RIGHT_MIN_DX   60
#define SWIPE_MAX_DY         50

static lv_point_t g_press_pt;
static bool       g_press_valid;

static void on_screen_swipe(lv_event_t *e)
{
  lv_event_code_t code  = lv_event_get_code(e);
  lv_indev_t     *indev = lv_indev_active();
  lv_point_t      p;

  if (indev == NULL)
    {
      return;
    }

  lv_indev_get_point(indev, &p);

  if (code == LV_EVENT_PRESSED)
    {
      g_press_pt   = p;
      g_press_valid = true;
    }
  else if (code == LV_EVENT_RELEASED && g_press_valid)
    {
      int dx = p.x - g_press_pt.x;
      int dy = p.y - g_press_pt.y;

      g_press_valid = false;

      if (dx > SWIPE_RIGHT_MIN_DX &&
          (dy > -SWIPE_MAX_DY && dy < SWIPE_MAX_DY))
        {
          printf("[app] swipe right -> home (dx=%d dy=%d)\n", dx, dy);
          close_module();
        }
    }
}

/****************************************************************************
 * 主屏
 ****************************************************************************/

static lv_obj_t *make_home_btn(lv_obj_t *parent, int idx, int x, int y,
                               int w, int h)
{
  lv_obj_t *btn = lv_obj_create(parent);
  lv_obj_t *lbl;

  lv_obj_set_size(btn, w, h);
  lv_obj_set_pos(btn, x, y);
  lv_obj_set_style_bg_color(btn, lv_color_hex(g_mod_colors[idx]), 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_30, 0);
  lv_obj_set_style_radius(btn, 14, 0);
  lv_obj_set_style_border_width(btn, 2, 0);
  lv_obj_set_style_border_color(btn, lv_color_hex(g_mod_colors[idx]), 0);
  lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(btn, on_module_click, LV_EVENT_CLICKED,
                      (void *)(intptr_t)idx);

  lbl = lv_label_create(btn);
  lv_label_set_text(lbl, g_modules[idx]->name);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(lbl, lv_color_hex(COLOR_TEXT), 0);
  lv_obj_center(lbl);

  return btn;
}

static void build_home(void)
{
  lv_obj_t *scr  = lv_screen_active();
  lv_obj_t *lbl;
  int i;

  g_home = lv_obj_create(scr);
  lv_obj_set_size(g_home, 390, 450);
  lv_obj_set_pos(g_home, 0, 0);
  lv_obj_set_style_bg_color(g_home, lv_color_hex(COLOR_BG), 0);
  lv_obj_set_style_border_width(g_home, 0, 0);

  lbl = lv_label_create(g_home);
  lv_label_set_text(lbl, "Aura-Space");
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(lbl, lv_color_hex(COLOR_TEXT), 0);
  lv_obj_set_pos(lbl, 104, 40);

  lbl = lv_label_create(g_home);
  lv_label_set_text(lbl, "Focus Training System");
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl, lv_color_hex(COLOR_DIM), 0);
  lv_obj_set_pos(lbl, 98, 76);

  for (i = 0; i < 4; i++)
    {
      int x = 45 + (i % 2) * 160;
      int y = 130 + (i / 2) * 140;
      make_home_btn(g_home, i, x, y, 140, 110);
    }
}

/****************************************************************************
 * 入口
 ****************************************************************************/

int aura_app_run(void)
{
  lv_nuttx_dsc_t    dsc;
  lv_nuttx_result_t result;

  g_modules[0] = &g_beatflicks_module;
  g_modules[1] = &g_nback_module;
  g_modules[2] = &g_focus_module;
  g_modules[3] = &g_coach_module;

  lv_init();

  lv_nuttx_dsc_init(&dsc);
  dsc.fb_path    = "/dev/lcd0";
  dsc.input_path = "/dev/input0";

  lv_nuttx_init(&dsc, &result);
  if (result.disp == NULL)
    {
      printf("[app] lvgl init failed\n");
      return -1;
    }

  build_home();
  printf("[app] Aura-Space running (4 modules)\n");

  for (; ; )
    {
      uint32_t now = aura_millis();

      if (g_active && g_active->tick)
        {
          g_active->tick(now);
        }

      lv_timer_handler();
      usleep(16000);   /* ~60FPS */
    }

  return 0;
}
