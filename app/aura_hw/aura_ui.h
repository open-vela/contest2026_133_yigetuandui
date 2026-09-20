/****************************************************************************
 * aura_hw/aura_ui.h
 * BeatFlicks 模块（LVGL 界面 + 手势/触屏双输入 + 音乐）
 ****************************************************************************/

#ifndef AURA_HW_AURA_UI_H
#define AURA_HW_AURA_UI_H

/* 订阅 BeatFlicks 所需的 EventBus 事件（在 game_port_start 之前调用） */
void aura_ui_init_events(void);

#endif /* AURA_HW_AURA_UI_H */
