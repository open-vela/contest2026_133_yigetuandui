/****************************************************************************
 * aura_hw/game_port.h
 * 游戏引擎胶水层：EventBus 手势事件 → 判定系统 → 振动/结算/存储
 ****************************************************************************/

#ifndef AURA_HW_GAME_PORT_H
#define AURA_HW_GAME_PORT_H

#include <stdint.h>

/* 单次判定事件（AURA_EVENT_JUDGE 的 payload） */
typedef struct
{
  int32_t note_index;   /* 被消费的音符下标 */
  int32_t result;       /* JudgeResult：1=Perfect 2=Good 3=Miss */
  int32_t delta_ms;     /* 手势与音符的时间差（超时 Miss 为 0） */
} aura_judge_info_t;

/* beatmap_json 传 NULL 使用内置演示谱面 */
int  game_port_start(const char *beatmap_json);
void game_port_stop(void);

#endif /* AURA_HW_GAME_PORT_H */
