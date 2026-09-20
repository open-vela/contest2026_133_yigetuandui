/****************************************************************************
 * aura_hw/lsm6ds3.h
 * LSM6DS3TR-C 六轴 IMU 寄存器级驱动（对应方案 3.3 第一节）
 *
 * 黄山派板载该芯片（SiFli Wiki 确认），经 I2C 字符设备 /dev/i2cN 直调，
 * 绕过快应用传感器框架的节流限制。
 ****************************************************************************/

#ifndef AURA_HW_LSM6DS3_H
#define AURA_HW_LSM6DS3_H

#include <stdint.h>
#include <stdbool.h>

/* 一帧原始物理量（对上层只暴露换算后的单位） */
typedef struct
{
  uint32_t timestamp_ms;   /* 采集时刻（CLOCK_MONOTONIC 毫秒） */
  float    gyro_dps[3];    /* 角速度 °/s：x / y / z */
  float    accel_g[3];     /* 加速度 g：x / y / z */
} imu_raw_frame_t;

typedef struct
{
  int     fd;              /* /dev/i2cN 句柄 */
  uint8_t addr;            /* 7bit 地址：0x6A（SA0 低）/ 0x6B（SA0 高） */
  bool    ready;
} lsm6ds3_dev_t;

int  lsm6ds3_init(lsm6ds3_dev_t *dev, const char *i2c_path, uint8_t addr);
int  lsm6ds3_set_odr_200hz(lsm6ds3_dev_t *dev);   /* 正常档（方案验收基准） */
int  lsm6ds3_set_odr_100hz(lsm6ds3_dev_t *dev);   /* 降级档（方案 3.3 降级方案） */
int  lsm6ds3_read_frame(lsm6ds3_dev_t *dev, imu_raw_frame_t *frame);
void lsm6ds3_deinit(lsm6ds3_dev_t *dev);

#endif /* AURA_HW_LSM6DS3_H */
