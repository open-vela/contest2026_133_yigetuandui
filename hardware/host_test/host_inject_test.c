/*
 * host_inject_test.c —— 手势识别准确率宿主仿真（无需开发板）
 * 注入 100 组合成 IMU 波形，验证 gesture_recognizer + imu_service 滤波管线
 * 对应验收标准：识别准确率 >= 90%
 */
/* 宿主验证：复刻 aura_main.c 的 inject 测试，验证手势识别准确率 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <stdarg.h>
#include "event_bus.h"
#include "imu_service.h"
#include "gesture_recognizer.h"

/* NuttX ioctl 桩 */
int ioctl(int fd, int req, ...) { (void)fd; (void)req; return -1; }

#define INJECT_WINDOW 40
#define INJECT_IDLE   80
#define INJECT_CASES  100

static int  g_hit_type;
static int  g_hit_flag;

static void on_gesture_count(aura_event_type_t type, const void *payload, void *user)
{
  const gesture_event_t *ev = payload;
  (void)type; (void)user;
  g_hit_type = ev->type;
  g_hit_flag = 1;
}

static uint32_t g_lfsr = 0xace1u;
static float inject_noise(void)
{
  g_lfsr = g_lfsr * 1664525u + 1013904223u;
  return (float)((int)((g_lfsr >> 8) % 200u) - 100) * 0.1f;
}

static void build_waveform(imu_frame_t *frames, int n, int type)
{
  int i;
  for (i = 0; i < n; i++)
    {
      float gx = inject_noise(), gy = inject_noise(), gz = inject_noise(), env;
      if (type == 0)      { env = expf(-((float)(i-20)*(i-20))/18.0f);  gx += 300.0f*env; }
      else if (type == 1) { env = expf(-((float)(i-20)*(i-20))/128.0f); gy += 400.0f*env; }
      else                { env = expf(-((float)(i-20)*(i-20))/128.0f); gy -= 400.0f*env; }
      frames[i].timestamp_ms = 0;
      frames[i].gyro_dps[0]=gx; frames[i].gyro_dps[1]=gy; frames[i].gyro_dps[2]=gz;
      frames[i].accel_g[0]=inject_noise()*0.001f;
      frames[i].accel_g[1]=inject_noise()*0.001f;
      frames[i].accel_g[2]=1.0f+inject_noise()*0.001f;
      frames[i].pitch_deg=0.0f; frames[i].roll_deg=0.0f;
    }
}

int main(void)
{
  static imu_frame_t frames[INJECT_WINDOW], idle[INJECT_IDLE];
  int per_hit[3]={0,0,0}, per_total[3]={0,0,0}, total_hit=0, i;

  memset(idle, 0, sizeof(idle));
  for (i = 0; i < INJECT_IDLE; i++)
    {
      idle[i].gyro_dps[0]=inject_noise(); idle[i].gyro_dps[1]=inject_noise();
      idle[i].gyro_dps[2]=inject_noise(); idle[i].accel_g[2]=1.0f;
    }

  aura_event_bus_init();
  imu_service_start_mock(gesture_on_frame, NULL);
  gesture_recognizer_init();
  aura_event_subscribe(AURA_EVENT_GESTURE, on_gesture_count, NULL);

  for (i = 0; i < INJECT_CASES; i++)
    {
      int type = i % 3;
      per_total[type]++;
      build_waveform(frames, INJECT_WINDOW, type);
      g_hit_flag = 0; g_hit_type = -1;
      imu_service_inject(frames, INJECT_WINDOW, 5);
      imu_service_inject(idle, INJECT_IDLE, 5);
      if (g_hit_flag && g_hit_type == type) { per_hit[type]++; total_hit++; }
    }

  printf("up %d/%d  left %d/%d  right %d/%d\n",
         per_hit[0],per_total[0],per_hit[1],per_total[1],per_hit[2],per_total[2]);
  printf("total accuracy %d/%d = %.1f%%\n", total_hit, INJECT_CASES, total_hit*100.0f/INJECT_CASES);
  return 0;
}
