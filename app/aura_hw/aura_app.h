/****************************************************************************
 * aura_hw/aura_app.h
 * Aura-Space 手表应用外壳：LVGL 初始化 + 主屏四模块入口 + 触屏导航
 ****************************************************************************/

#ifndef AURA_HW_AURA_APP_H
#define AURA_HW_AURA_APP_H

#include <stdint.h>
#include <lvgl/lvgl.h>

/* 模块接口：进入时 create，退出时 delete，主循环周期 tick */
typedef struct
{
  const char *name;
  void (*create)(lv_obj_t *parent);
  void (*delete)(void);
  void (*tick)(uint32_t now_ms);
} aura_module_t;

/* 初始化 LVGL 与显示屏，进入应用主循环（阻塞） */
int aura_app_run(void);

/* 各模块入口（分散在各自文件中实现） */
extern const aura_module_t g_beatflicks_module;
extern const aura_module_t g_nback_module;
extern const aura_module_t g_focus_module;
extern const aura_module_t g_coach_module;

#endif /* AURA_HW_AURA_APP_H */
