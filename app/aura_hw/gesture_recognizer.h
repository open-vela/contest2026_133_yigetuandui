/****************************************************************************
 * aura_hw/gesture_recognizer.h
 * 甩腕手势识别（对应方案 3.3 第二节：滑窗 + 双阈值 + FSM + 误触过滤）
 ****************************************************************************/

#ifndef AURA_HW_GESTURE_RECOGNIZER_H
#define AURA_HW_GESTURE_RECOGNIZER_H

#include <stdint.h>

#include "imu_service.h"

/* 手势类型取值与游戏引擎 NoteType 一一对应（0/1/2），
 * EventBus 直传判定系统时无需映射（方案 3.1 音符类型枚举）。
 */
enum
{
  GESTURE_UP_TOSS     = 0,   /* 抬腕上抛：X 轴角速度正向脉冲 */
  GESTURE_LEFT_FLICK  = 1,   /* 外旋左甩：Y 轴正向脉冲 */
  GESTURE_RIGHT_FLICK = 2,   /* 内旋右甩：Y 轴负向脉冲 */
  GESTURE_NONE        = -1
};

/* 手势事件（方案 3.3 阶段七：类型 + 时间戳 + 置信度） */
typedef struct
{
  int      type;           /* GESTURE_* 枚举 */
  uint32_t timestamp_ms;   /* 触发时刻（系统单调时钟） */
  float    confidence;     /* 0.0 ~ 1.0，<0.7 由上层丢弃（方案 3.4） */
} gesture_event_t;

int  gesture_recognizer_init(void);
void gesture_recognizer_reset(void);

/* 逐帧消费入口：注册为 imu_service 的回调 */
void gesture_on_frame(const imu_frame_t *frame, void *user);

#endif /* AURA_HW_GESTURE_RECOGNIZER_H */
