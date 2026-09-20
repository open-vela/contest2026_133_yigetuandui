/****************************************************************************
 * aura_hw/imu_service.c
 ****************************************************************************/

#include <nuttx/config.h>

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "util.h"
#include "lsm6ds3.h"
#include "imu_service.h"

#ifndef CONFIG_EXAMPLES_AURA_HW_IMU_PATH
#  define CONFIG_EXAMPLES_AURA_HW_IMU_PATH  "/dev/i2c1"
#endif

#ifndef CONFIG_EXAMPLES_AURA_HW_IMU_ADDR
#  define CONFIG_EXAMPLES_AURA_HW_IMU_ADDR  0x6a
#endif

#define RING_SIZE           64          /* 2 的幂，≥ 40 帧手势窗口 */
#define RING_MASK           (RING_SIZE - 1)
#define SAMPLE_PERIOD_NS    5000000L    /* 200Hz = 5ms */
#define DEGRADE_PERIOD_NS   10000000L   /* 100Hz = 10ms（降级档） */

#define RAD2DEG             57.2958f
#define COMPLEMENT_ALPHA    0.98f       /* 方案 3.3：陀螺仪积分主导 */
#define ACCEL_LPF_ALPHA     0.386f      /* 一阶低通 fc≈20Hz @200Hz */

static lsm6ds3_dev_t    g_dev;
static imu_frame_t      g_ring[RING_SIZE];
static volatile uint32_t g_head;        /* 环形缓冲写索引 */
static imu_frame_cb_t   g_cb;
static void            *g_cb_user;
static pthread_t        g_thread;
static volatile bool    g_running;
static bool             g_mock;
static long             g_period_ns = SAMPLE_PERIOD_NS;

/* 滤波状态 */
static float            g_pitch;
static float            g_roll;
static float            g_bias[3];      /* 陀螺仪零偏慢速估计 */
static float            g_lpf[3];       /* 加速度低通状态 */
static uint32_t         g_last_ts;

/* 健康监测（方案 3.4 故障检测状态机） */
static int              g_fail_count;
static bool             g_degraded;
static bool             g_safe_mode;

/* 采样抖动统计 */
static uint32_t         g_jitter_max_us;
static uint64_t         g_jitter_sum_us;
static uint32_t         g_jitter_cnt;

/* 注入模式虚拟时间轴（文件作用域，便于复位） */
static uint32_t         g_inject_vts;

/* 复位全部运行态。
 * 注意：NuttX 平坦地址空间下，NSH 多次调用 aura 命令时
 * 文件静态变量会跨任务残留（上次 inject 的虚拟时钟会导致本次
 * 首帧 dt 下溢溢出）。每次启动服务必须显式复位。
 */
static void imu_state_reset(void)
{
  g_head       = 0;
  g_last_ts    = 0;
  g_pitch      = 0.0f;
  g_roll       = 0.0f;
  g_inject_vts = 0;

  memset(g_bias, 0, sizeof(g_bias));
  memset(g_lpf, 0, sizeof(g_lpf));

  g_fail_count   = 0;
  g_degraded     = false;
  g_safe_mode    = false;
  g_jitter_max_us = 0;
  g_jitter_sum_us = 0;
  g_jitter_cnt    = 0;
}

/* 帧处理管线：滤波 → 入环形缓冲 → 回调手势识别 */
static void process_frame(imu_frame_t *f)
{
  float dt;
  float acc_pitch;
  float acc_roll;
  bool at_rest;
  int i;

  /* 1. 陀螺仪零偏高通：三轴均静止（|ω|<5°/s）时慢速更新零偏再扣除，
   *    消除零偏长期漂移（方案 3.3 数据融合 a/c）
   */
  at_rest = fabsf(f->gyro_dps[0]) < 5.0f &&
            fabsf(f->gyro_dps[1]) < 5.0f &&
            fabsf(f->gyro_dps[2]) < 5.0f;
  for (i = 0; i < 3; i++)
    {
      if (at_rest)
        {
          g_bias[i] = 0.999f * g_bias[i] + 0.001f * f->gyro_dps[i];
        }

      f->gyro_dps[i] -= g_bias[i];
    }

  /* 2. 加速度一阶低通，平滑高频振动噪声（方案 3.3 数据融合 b） */
  for (i = 0; i < 3; i++)
    {
      g_lpf[i] += ACCEL_LPF_ALPHA * (f->accel_g[i] - g_lpf[i]);
      f->accel_g[i] = g_lpf[i];
    }

  /* 3. 互补滤波：α=0.98 陀螺仪积分 + 0.02 加速度修正俯仰/横滚 */
  dt = g_last_ts ? (float)(f->timestamp_ms - g_last_ts) / 1000.0f
                 : 0.005f;
  g_last_ts = f->timestamp_ms;

  acc_pitch = atan2f(f->accel_g[1], f->accel_g[2]) * RAD2DEG;
  acc_roll  = atan2f(-f->accel_g[0],
                     sqrtf(f->accel_g[1] * f->accel_g[1] +
                           f->accel_g[2] * f->accel_g[2])) * RAD2DEG;

  g_pitch = COMPLEMENT_ALPHA * (g_pitch + f->gyro_dps[0] * dt) +
            (1.0f - COMPLEMENT_ALPHA) * acc_pitch;
  g_roll  = COMPLEMENT_ALPHA * (g_roll + f->gyro_dps[1] * dt) +
            (1.0f - COMPLEMENT_ALPHA) * acc_roll;

  f->pitch_deg = g_pitch;
  f->roll_deg  = g_roll;

  /* 4. 入环形缓冲 + 在线回调（手势识别器逐帧消费，延迟 <5ms） */
  g_ring[g_head & RING_MASK] = *f;
  g_head++;

  if (g_cb)
    {
      g_cb(f, g_cb_user);
    }
}

static void *imu_sampler(void *arg)
{
  struct timespec next;
  struct timespec now;
  imu_raw_frame_t raw;
  imu_frame_t f;
  int64_t last_us = 0;
  int64_t now_us;
  uint32_t expect_us;
  uint32_t dev_us;

  (void)arg;

  clock_gettime(CLOCK_MONOTONIC, &next);

  while (g_running)
    {
      /* 绝对时间调度：周期不累加睡眠误差，抖动只取决于唤醒精度 */
      next.tv_nsec += g_period_ns;
      if (next.tv_nsec >= 1000000000L)
        {
          next.tv_sec++;
          next.tv_nsec -= 1000000000L;
        }

      clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);

      /* 采样抖动统计（方案 3.3 性能分析 a） */
      clock_gettime(CLOCK_MONOTONIC, &now);
      now_us = (int64_t)now.tv_sec * 1000000LL + now.tv_nsec / 1000;
      if (last_us != 0)
        {
          expect_us = (uint32_t)(g_period_ns / 1000);
          dev_us = (now_us - last_us) > (int64_t)expect_us
                 ? (uint32_t)(now_us - last_us) - expect_us
                 : expect_us - (uint32_t)(now_us - last_us);
          g_jitter_sum_us += dev_us;
          g_jitter_cnt++;
          if (dev_us > g_jitter_max_us)
            {
              g_jitter_max_us = dev_us;
            }
        }

      last_us = now_us;

      if (lsm6ds3_read_frame(&g_dev, &raw) == 0)
        {
          g_fail_count = 0;
          f.timestamp_ms = raw.timestamp_ms;
          memcpy(f.gyro_dps, raw.gyro_dps, sizeof(f.gyro_dps));
          memcpy(f.accel_g, raw.accel_g, sizeof(f.accel_g));
          process_frame(&f);
        }
      else
        {
          /* 故障检测（方案 3.4）：I2C 连续失败 → 降级 100Hz → 安全模式 */
          g_fail_count++;
          if (g_fail_count >= 3 && !g_degraded)
            {
              g_degraded   = true;
              g_fail_count = 0;
              g_period_ns  = DEGRADE_PERIOD_NS;
              lsm6ds3_set_odr_100hz(&g_dev);
              printf("[imu] I2C 连续失败，降级到 100Hz 采样\n");
            }
          else if (g_fail_count >= 3 && g_degraded && !g_safe_mode)
            {
              g_safe_mode = true;
              printf("[imu] IMU 不可用，进入安全模式"
                     "（应禁用体感音游，仅保留 N-Back/看板）\n");
            }
        }
    }

  return NULL;
}

int imu_service_start(imu_frame_cb_t cb, void *user)
{
  g_cb      = cb;
  g_cb_user = user;
  g_mock    = false;

  imu_state_reset();

  if (lsm6ds3_init(&g_dev, CONFIG_EXAMPLES_AURA_HW_IMU_PATH,
                   CONFIG_EXAMPLES_AURA_HW_IMU_ADDR) < 0)
    {
      return -1;
    }

  g_running = true;
  return pthread_create(&g_thread, NULL, imu_sampler, NULL);
}

int imu_service_start_mock(imu_frame_cb_t cb, void *user)
{
  g_cb      = cb;
  g_cb_user = user;
  g_mock    = true;
  g_running = false;   /* 不启动硬件线程，由 inject 同步驱动 */

  imu_state_reset();
  return 0;
}

void imu_service_stop(void)
{
  g_running = false;
  if (!g_mock)
    {
      pthread_join(g_thread, NULL);
      lsm6ds3_deinit(&g_dev);
    }
}

int imu_service_get_window(imu_frame_t *out, int max_frames)
{
  uint32_t head = g_head;
  int avail;
  int n;
  int i;

  avail = head > RING_SIZE ? RING_SIZE : (int)head;
  n     = max_frames < avail ? max_frames : avail;

  /* 输出按时间升序。消费端为只读拷贝，允许偶发读到旧一帧；
   * 手势识别基于 40 帧统计特征，对单帧竞争不敏感。
   */
  for (i = 0; i < n; i++)
    {
      out[i] = g_ring[(head - n + i) & RING_MASK];
    }

  return n;
}

int imu_service_inject(const imu_frame_t *frames, int count,
                       uint32_t step_ms)
{
  int i;

  if (!g_mock)
    {
      return -1;
    }

  for (i = 0; i < count; i++)
    {
      imu_frame_t f = frames[i];
      f.timestamp_ms = g_inject_vts;
      g_inject_vts += step_ms;
      process_frame(&f);
    }

  return 0;
}

bool imu_service_healthy(void)
{
  return !g_safe_mode;
}

bool imu_service_safe_mode(void)
{
  return g_safe_mode;
}

void imu_service_jitter_stats(uint32_t *mean_us, uint32_t *max_us)
{
  if (mean_us)
    {
      *mean_us = g_jitter_cnt
               ? (uint32_t)(g_jitter_sum_us / g_jitter_cnt) : 0;
    }

  if (max_us)
    {
      *max_us = g_jitter_max_us;
    }
}
