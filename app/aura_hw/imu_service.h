/****************************************************************************
 * aura_hw/imu_service.h
 * IMU 采样服务（对应方案 3.3 第一/四节 + 3.4 故障降级）
 *
 * 职责：
 *   - 200Hz 周期采样线程（绝对时间调度，防漂移）+ 采样抖动统计
 *   - 陀螺仪零偏高通 + 加速度低通 + 互补滤波姿态（α=0.98）
 *   - 64 帧环形缓冲（≥40 帧手势窗口）
 *   - I2C 连续失败 → 自动降级 100Hz → 安全模式（方案 3.4 故障检测）
 *   - 波形注入接口（方案 3.4 集成测试：imu_inject_waveform）
 ****************************************************************************/

#ifndef AURA_HW_IMU_SERVICE_H
#define AURA_HW_IMU_SERVICE_H

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
  uint32_t timestamp_ms;
  float    gyro_dps[3];    /* 去零偏后的角速度 °/s */
  float    accel_g[3];     /* 低通后的加速度 g */
  float    pitch_deg;      /* 互补滤波姿态输出 */
  float    roll_deg;
} imu_frame_t;

typedef void (*imu_frame_cb_t)(const imu_frame_t *frame, void *user);

/* 真机模式：打开 I2C 并启动 200Hz 采样线程，每帧回调 cb */
int  imu_service_start(imu_frame_cb_t cb, void *user);

/* 注入模式：不碰硬件，由 imu_service_inject() 驱动（PC 仿真/集成测试用） */
int  imu_service_start_mock(imu_frame_cb_t cb, void *user);

void imu_service_stop(void);

/* 读取最近 max_frames 帧（时间升序拷贝），返回实际帧数 */
int  imu_service_get_window(imu_frame_t *out, int max_frames);

/* 集成测试注入：按 step_ms 间隔依次推入 frames（仅 mock 模式） */
int  imu_service_inject(const imu_frame_t *frames, int count,
                        uint32_t step_ms);

/* 健康状态（方案 3.4：安全模式下上层应禁用体感音游） */
bool imu_service_healthy(void);
bool imu_service_safe_mode(void);

/* 采样抖动统计（方案 3.3 性能分析：目标均值 <0.5ms，最大 <2ms） */
void imu_service_jitter_stats(uint32_t *mean_us, uint32_t *max_us);

#endif /* AURA_HW_IMU_SERVICE_H */
