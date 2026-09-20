/****************************************************************************
 * aura_hw/event_bus.h
 * C 版 EventBus（对应方案 3.4：静态订阅表 + 观察者模式）
 ****************************************************************************/

#ifndef AURA_HW_EVENT_BUS_H
#define AURA_HW_EVENT_BUS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* 事件类型（对应方案 3.4 的事件分类） */
typedef enum
{
  AURA_EVENT_GESTURE = 0,   /* payload: gesture_event_t  手势识别 → 判定系统 */
  AURA_EVENT_JUDGE,         /* payload: aura_judge_info_t 判定 → UI / 振动 */
  AURA_EVENT_GAME_RESULT,   /* payload: GameResultEvent   结算 → 数据层 / UI */
  AURA_EVENT_ROUND_START,   /* payload: uint32_t 本局起始时刻 → UI 同步 */
  AURA_EVENT_MAX
} aura_event_type_t;

typedef void (*aura_event_handler_t)(aura_event_type_t type,
                                     const void *payload,
                                     void *user_data);

int aura_event_bus_init(void);
int aura_event_subscribe(aura_event_type_t type,
                         aura_event_handler_t handler, void *user_data);
int aura_event_publish(aura_event_type_t type, const void *payload);

#ifdef __cplusplus
}
#endif

#endif /* AURA_HW_EVENT_BUS_H */
