/****************************************************************************
 * aura_hw/util.h
 * 公共小工具：统一毫秒时钟（方案 3.4 系统时间同步的落点）
 ****************************************************************************/

#ifndef AURA_HW_UTIL_H
#define AURA_HW_UTIL_H

#include <stdint.h>
#include <time.h>

/* 系统单调时钟毫秒值。全链路（IMU 采样 / 手势事件 / 判定 / 结算）
 * 统一使用此时钟，避免多时钟源换算误差。
 */
static inline uint32_t aura_millis(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint32_t)(ts.tv_sec * 1000u) + (uint32_t)(ts.tv_nsec / 1000000u);
}

#endif /* AURA_HW_UTIL_H */
