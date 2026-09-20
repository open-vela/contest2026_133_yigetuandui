/****************************************************************************
 * aura_hw/vib_motor.h
 * PWM 振动马达控制（对应方案 3.3 第三节 + 3.4 振动调度）
 ****************************************************************************/

#ifndef AURA_HW_VIB_MOTOR_H
#define AURA_HW_VIB_MOTOR_H

/* 五种预设振动模式（方案 3.3 时序表） */
typedef enum
{
  VIB_PATTERN_SINGLE_SHORT = 1,   /* 50ms 60%：Miss / 走神轻唤醒 */
  VIB_PATTERN_DOUBLE_SHORT = 2,   /* 50+50+50ms 70%：Good */
  VIB_PATTERN_TRIPLE_SHORT = 3,   /* 50ms×3 80%：N-Back 触觉编码 */
  VIB_PATTERN_LONG_STRONG  = 4,   /* 100ms 100%：Perfect / 强唤醒 */
  VIB_PATTERN_DUAL_FREQ    = 5    /* 高频+低频交替：N-Back 双属性匹配 */
} vib_pattern_id_t;

/* 请求优先级（方案 3.4：系统告警 > 游戏判定 > N-Back，高可抢占低） */
typedef enum
{
  VIB_PRIO_NBACK  = 0,
  VIB_PRIO_GAME   = 1,
  VIB_PRIO_SYSTEM = 2
} vib_priority_t;

int  vib_motor_init(void);
int  vib_request(vib_pattern_id_t pattern, vib_priority_t prio);
void vib_stop_all(void);
void vib_motor_deinit(void);

#endif /* AURA_HW_VIB_MOTOR_H */
