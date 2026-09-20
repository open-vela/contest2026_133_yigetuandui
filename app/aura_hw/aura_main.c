/****************************************************************************
 * aura_hw/aura_main.c
 * Aura-Space 黄山派板端 NSH 入口
 *
 * 用法：
 *   aura            / aura demo     全链路演示：IMU→手势→判定→振动→五维存储
 *   aura imu                        10Hz 打印 IMU 融合数据（bring-up 用）
 *   aura gesture                    打印识别到的手势与置信度（阈值标定用）
 *   aura vib <1-5>                  播放指定振动模式
 *   aura inject                     注入 100 组合成波形，输出识别准确率
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#include "util.h"
#include "event_bus.h"
#include "imu_service.h"
#include "gesture_recognizer.h"
#include "vib_motor.h"
#include "radar_store.h"
#include "game_port.h"
#include "music.h"
#include "aura_ui.h"
#include "aura_app.h"

#include "game_engine.h"   /* GameResultEvent */

static const char *g_gesture_names[] = { "上抛", "左甩", "右甩" };
static const char *g_judge_names[]   = { "", "Perfect", "Good", "Miss" };

/****************************************************************************
 * 事件打印回调
 ****************************************************************************/

static void on_gesture_print(aura_event_type_t type, const void *payload,
                             void *user)
{
  const gesture_event_t *ev = payload;

  (void)type;
  (void)user;

  if (ev->type >= 0 && ev->type <= 2)
    {
      printf("[gesture] t=%ums %s conf=%.2f\n",
             (unsigned)ev->timestamp_ms, g_gesture_names[ev->type],
             ev->confidence);
    }
}

static void on_judge_print(aura_event_type_t type, const void *payload,
                           void *user)
{
  const aura_judge_info_t *info = payload;

  (void)type;
  (void)user;

  if (info->result >= 1 && info->result <= 3)
    {
      printf("[judge] note#%d -> %s (delta=%dms)\n",
             (int)info->note_index, g_judge_names[info->result],
             (int)info->delta_ms);
    }
}

static void on_result_print(aura_event_type_t type, const void *payload,
                            void *user)
{
  const GameResultEvent *r = payload;

  (void)type;
  (void)user;

  printf("[result] score=%u max_combo=%u acc=%.1f%% grade=%c\n",
         (unsigned)r->final_score, (unsigned)r->max_combo,
         r->accuracy * 100.0f, r->grade);
  radar_store_print();
}

static void on_frame_print(const imu_frame_t *f, void *user)
{
  static int decim;

  (void)user;

  if (++decim % 20 != 0)
    {
      return;   /* 200Hz → 10Hz 抽稀打印 */
    }

  printf("[imu] t=%u gyro=(%.1f,%.1f,%.1f)dps accel=(%.2f,%.2f,%.2f)g"
         " pitch=%.1f roll=%.1f\n",
         (unsigned)f->timestamp_ms,
         f->gyro_dps[0], f->gyro_dps[1], f->gyro_dps[2],
         f->accel_g[0], f->accel_g[1], f->accel_g[2],
         f->pitch_deg, f->roll_deg);
}

/* 校准回调：每 250ms 打印一次窗口内三轴角速度峰值，
 * 用户照常用三种手势甩腕，即可读出真实峰值用于定阈值
 */
static void calib_cb(const imu_frame_t *f, void *user)
{
  static float    peak[3];
  static uint32_t last_print;
  int             i;

  (void)user;

  for (i = 0; i < 3; i++)
    {
      float a = fabsf(f->gyro_dps[i]);
      if (a > peak[i])
        {
          peak[i] = a;
        }
    }

  if (f->timestamp_ms - last_print >= 250)
    {
      last_print = f->timestamp_ms;
      printf("[calib] peak gx=%.0f gy=%.0f gz=%.0f dps\n",
             peak[0], peak[1], peak[2]);
      peak[0] = peak[1] = peak[2] = 0.0f;
    }
}

/****************************************************************************
 * 注入测试：100 组合成波形 → 识别准确率（对应验收标准 ≥90%）
 ****************************************************************************/

#define INJECT_WINDOW   40    /* 与手势窗口一致 */
#define INJECT_IDLE     80    /* 手势后 400ms 空闲帧：越冷却 + 重填窗口 */
#define INJECT_CASES    100

static int  g_hit_type;       /* 本用例识别到的手势 */
static bool g_hit_flag;

static void on_gesture_count(aura_event_type_t type, const void *payload,
                             void *user)
{
  const gesture_event_t *ev = payload;

  (void)type;
  (void)user;

  g_hit_type = ev->type;
  g_hit_flag = true;
}

/* 确定性伪随机（LCG）：注入测试不依赖 rand() 种子，保证可复现 */
static uint32_t g_lfsr = 0xace1u;

static float inject_noise(void)
{
  g_lfsr = g_lfsr * 1664525u + 1013904223u;
  /* 注意先转 int 再减 100：直接对无符号数做减法会下溢回绕 */
  return (float)((int)((g_lfsr >> 8) % 200u) - 100) * 0.1f;   /* ±10°/s */
}

static void build_waveform(imu_frame_t *frames, int n, int type)
{
  int i;

  for (i = 0; i < n; i++)
    {
      float gx = inject_noise();
      float gy = inject_noise();
      float gz = inject_noise();
      float env;

      /* 高斯包络脉冲：中心第 20 帧。上抛窄脉冲（σ≈3），甩腕宽脉冲（σ=8，
       * 保证 40 帧均值 >150°/s 的双阈值要求）
       */
      if (type == GESTURE_UP_TOSS)
        {
          env  = expf(-((float)(i - 20) * (i - 20)) / 18.0f);
          gx  += 300.0f * env;
        }
      else if (type == GESTURE_LEFT_FLICK)
        {
          env  = expf(-((float)(i - 20) * (i - 20)) / 128.0f);
          gy  += 400.0f * env;
        }
      else
        {
          env  = expf(-((float)(i - 20) * (i - 20)) / 128.0f);
          gy  -= 400.0f * env;
        }

      frames[i].timestamp_ms = 0;
      frames[i].gyro_dps[0]  = gx;
      frames[i].gyro_dps[1]  = gy;
      frames[i].gyro_dps[2]  = gz;
      frames[i].accel_g[0]   = inject_noise() * 0.001f;
      frames[i].accel_g[1]   = inject_noise() * 0.001f;
      frames[i].accel_g[2]   = 1.0f + inject_noise() * 0.001f;
      frames[i].pitch_deg    = 0.0f;
      frames[i].roll_deg     = 0.0f;
    }
}

static int run_inject_test(void)
{
  static imu_frame_t frames[INJECT_WINDOW];
  static imu_frame_t idle[INJECT_IDLE];
  int per_type_hit[3]  = { 0, 0, 0 };
  int per_type_total[3] = { 0, 0, 0 };
  int total_hit = 0;
  int i;

  memset(idle, 0, sizeof(idle));
  for (i = 0; i < INJECT_IDLE; i++)
    {
      idle[i].gyro_dps[0] = inject_noise();
      idle[i].gyro_dps[1] = inject_noise();
      idle[i].gyro_dps[2] = inject_noise();
      idle[i].accel_g[2]  = 1.0f;
    }

  imu_service_start_mock(gesture_on_frame, NULL);
  gesture_recognizer_init();
  aura_event_subscribe(AURA_EVENT_GESTURE, on_gesture_count, NULL);

  for (i = 0; i < INJECT_CASES; i++)
    {
      int type = i % 3;

      per_type_total[type]++;
      build_waveform(frames, INJECT_WINDOW, type);

      g_hit_flag = false;
      g_hit_type = GESTURE_NONE;

      imu_service_inject(frames, INJECT_WINDOW, 5);
      imu_service_inject(idle, INJECT_IDLE, 5);

      if (g_hit_flag && g_hit_type == type)
        {
          per_type_hit[type]++;
          total_hit++;
        }
    }

  printf("[inject] 上抛 %d/%d  左甩 %d/%d  右甩 %d/%d\n",
         per_type_hit[0], per_type_total[0],
         per_type_hit[1], per_type_total[1],
         per_type_hit[2], per_type_total[2]);
  printf("[inject] 总准确率 %d/%d = %.1f%%（验收目标 ≥90%%）\n",
         total_hit, INJECT_CASES,
         total_hit * 100.0f / INJECT_CASES);

  return 0;
}

/****************************************************************************
 * 子命令
 ****************************************************************************/

static int run_vib(int pattern)
{
  if (vib_motor_init() < 0)
    {
      return -1;
    }

  printf("[vib] play pattern %d\n", pattern);
  vib_request((vib_pattern_id_t)pattern, VIB_PRIO_SYSTEM);
  sleep(2);
  vib_motor_deinit();
  return 0;
}

static int run_demo(void)
{
  printf("=== Aura-Space 黄山派演示模式 ===\n");

  vib_motor_init();      /* 失败不致命：仿真环境无 PWM 节点 */
  radar_store_init();
  gesture_recognizer_init();

  aura_event_subscribe(AURA_EVENT_GESTURE, on_gesture_print, NULL);
  aura_event_subscribe(AURA_EVENT_JUDGE, on_judge_print, NULL);
  aura_event_subscribe(AURA_EVENT_GAME_RESULT, on_result_print, NULL);

  if (game_port_start(NULL) < 0)
    {
      return -1;
    }

  if (imu_service_start(gesture_on_frame, NULL) < 0)
    {
      printf("[main] IMU 初始化失败，判定引擎继续运行"
             "（可用 aura inject 离线验证）\n");
    }

  for (; ; )
    {
      sleep(1);
    }

  return 0;
}

/* Aura-Space 四模块应用：主屏导航 + BeatFlicks/Synapse-N/Focus/Coach */
static int run_app(void)
{
  printf("=== Aura-Space 四模块应用 ===\n");

  vib_motor_init();
  radar_store_init();
  gesture_recognizer_init();
  music_init();                   /* /dev/audio 缺失时自动降级视觉节拍 */

  aura_event_subscribe(AURA_EVENT_JUDGE, on_judge_print, NULL);
  aura_event_subscribe(AURA_EVENT_GAME_RESULT, on_result_print, NULL);
  aura_ui_init_events();          /* BeatFlicks 事件订阅 */

  if (imu_service_start(gesture_on_frame, NULL) < 0)
    {
      printf("[main] IMU 初始化失败，进入纯触屏模式\n");
    }

  return aura_app_run();          /* 阻塞渲染循环 */
}

int aura_main(int argc, char *argv[])
{
  const char *cmd = argc > 1 ? argv[1] : "demo";

  aura_event_bus_init();

  if (strcmp(cmd, "vib") == 0)
    {
      int pattern = argc > 2 ? atoi(argv[2]) : 4;
      if (pattern < 1 || pattern > 5)
        {
          printf("usage: aura vib <1-5>\n");
          return -1;
        }

      return run_vib(pattern);
    }
  else if (strcmp(cmd, "imu") == 0)
    {
      if (imu_service_start(on_frame_print, NULL) < 0)
        {
          return -1;
        }

      for (; ; )
        {
          sleep(1);
        }
    }
  else if (strcmp(cmd, "calib") == 0)
    {
      printf("校准模式：请连续做 上抛/左甩/右甩 各 5 次，观察峰值\n");
      if (imu_service_start(calib_cb, NULL) < 0)
        {
          return -1;
        }

      for (; ; )
        {
          sleep(1);
        }
    }
  else if (strcmp(cmd, "gesture") == 0)
    {
      gesture_recognizer_init();
      aura_event_subscribe(AURA_EVENT_GESTURE, on_gesture_print, NULL);
      if (imu_service_start(gesture_on_frame, NULL) < 0)
        {
          return -1;
        }

      for (; ; )
        {
          uint32_t mean_us;
          uint32_t max_us;
          sleep(10);
          imu_service_jitter_stats(&mean_us, &max_us);
          printf("[imu] jitter: mean=%uus max=%uus（目标 <500us / <2000us）\n",
                 (unsigned)mean_us, (unsigned)max_us);
        }
    }
  else if (strcmp(cmd, "inject") == 0)
    {
      return run_inject_test();
    }
  else if (strcmp(cmd, "app") == 0 || strcmp(cmd, "ui") == 0)
    {
      return run_app();
    }
  else if (strcmp(cmd, "demo") == 0)
    {
      return run_demo();
    }

  printf("usage: aura [app|demo|imu|gesture|calib|vib <1-5>|inject]\n");
  return -1;
}
