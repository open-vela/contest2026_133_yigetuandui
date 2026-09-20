/****************************************************************************
 * aura_hw/gesture_recognizer.c
 *
 * 判定流水线（方案 3.3 七阶段）：
 *   1. IMU 帧逐帧滑入 40 帧（200ms）窗口
 *   2. 每 10 帧（50ms 步长）触发一次判定
 *   3. 计算三轴峰值 / 均值 / 主方向占比
 *   4. 主方向判定（X 主导→上抛；Y 正→左甩；Y 负→右甩）
 *   5. 峰值 + 均值双阈值校验
 *   6. 误触过滤（走路 / 看表等周期性小动作）
 *   7. 封装 gesture_event_t 经 EventBus 推送，进入冷却复位
 ****************************************************************************/

#include "gesture_recognizer.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "util.h"
#include "event_bus.h"

/* ---- 判定参数（v2：按真机甩腕实测下调，精细标定见 aura calib） ---- */

#define WINDOW_FRAMES         40       /* 200ms @200Hz */
#define EVAL_STEP_FRAMES      10       /* 50ms 步长 */
#define THRESH_UP_PEAK_DPS    120.0f   /* 上抛：X 轴峰值（原 200 实测偏高） */
#define THRESH_FLICK_MEAN_DPS 80.0f    /* 左右甩：Y 轴均值（原 150） */
#define THRESH_FLICK_PEAK_DPS 120.0f   /* 左右甩：峰值交叉校验（原 180） */
#define DOMINANT_RATIO        0.50f    /* 主方向能量占比（原 0.60） */
#define MIN_ENERGY            100.0f   /* 窗口总能量下限，滤除静止噪声 */
#define COOLDOWN_MS           300      /* 确认后冷却，防同一手势连发 */
#define WALK_ACCEL_VAR        0.02f    /* 走路：加速度模长方差阈值（g²） */
#define WALK_SIGN_CHANGES     4        /* 走路：Y 轴角速度换向次数阈值 */

typedef enum
{
  G_IDLE = 0,
  G_COOLDOWN
} gstate_t;

static float    g_gx[WINDOW_FRAMES];
static float    g_gy[WINDOW_FRAMES];
static float    g_gz[WINDOW_FRAMES];
static float    g_amag[WINDOW_FRAMES];   /* 加速度模长（误触过滤用） */
static int      g_wlen;
static int      g_since_eval;
static gstate_t g_state;
static uint32_t g_cooldown_until;

/* 阶段三~六：窗口特征计算 + 判定 + 误触过滤 */
static int evaluate_window(float *confidence)
{
  float peak_x  = 0.0f;
  float peak_yp = 0.0f;
  float peak_yn = 0.0f;
  float sum_y   = 0.0f;
  float e_x     = 0.0f;
  float e_y     = 0.0f;
  float e_z     = 0.0f;
  float amean   = 0.0f;
  float avar    = 0.0f;
  float etotal;
  float mean_y;
  float share_x;
  float share_y;
  int   sign_changes = 0;
  int   last_sign    = 0;
  int   i;

  for (i = 0; i < g_wlen; i++)
    {
      if (g_gx[i] > peak_x)  peak_x  = g_gx[i];
      if (g_gy[i] > peak_yp) peak_yp = g_gy[i];
      if (g_gy[i] < peak_yn) peak_yn = g_gy[i];
      sum_y += g_gy[i];
      e_x   += fabsf(g_gx[i]);
      e_y   += fabsf(g_gy[i]);
      e_z   += fabsf(g_gz[i]);
      amean += g_amag[i];
    }

  etotal = e_x + e_y + e_z;
  if (etotal < MIN_ENERGY)
    {
      return GESTURE_NONE;   /* 静止/纯噪声 */
    }

  mean_y = sum_y / g_wlen;
  amean /= g_wlen;

  for (i = 0; i < g_wlen; i++)
    {
      float d = g_amag[i] - amean;
      avar += d * d;
    }

  avar /= g_wlen;

  /* Y 轴角速度带迟滞换向计数（走路的周期性摆动特征） */
  for (i = 0; i < g_wlen; i++)
    {
      int s = g_gy[i] > 30.0f ? 1 : (g_gy[i] < -30.0f ? -1 : 0);
      if (s != 0 && last_sign != 0 && s != last_sign)
        {
          sign_changes++;
        }

      if (s != 0)
        {
          last_sign = s;
        }
    }

  /* 阶段六：走路误触过滤——加速度周期性摆动 + 角速度多次换向 */
  if (avar > WALK_ACCEL_VAR && sign_changes >= WALK_SIGN_CHANGES)
    {
      return GESTURE_NONE;
    }

  share_x = e_x / etotal;
  share_y = e_y / etotal;

  /* 阶段四/五：主方向 + 峰值/均值双阈值 */
  if (share_x > DOMINANT_RATIO && peak_x > THRESH_UP_PEAK_DPS)
    {
      *confidence = fminf(1.0f, peak_x / (THRESH_UP_PEAK_DPS * 1.5f));
      return GESTURE_UP_TOSS;
    }

  if (share_y > DOMINANT_RATIO &&
      mean_y > THRESH_FLICK_MEAN_DPS && peak_yp > THRESH_FLICK_PEAK_DPS)
    {
      *confidence = fminf(1.0f, mean_y / (THRESH_FLICK_MEAN_DPS * 1.5f));
      return GESTURE_LEFT_FLICK;
    }

  if (share_y > DOMINANT_RATIO &&
      mean_y < -THRESH_FLICK_MEAN_DPS && peak_yn < -THRESH_FLICK_PEAK_DPS)
    {
      *confidence = fminf(1.0f, -mean_y / (THRESH_FLICK_MEAN_DPS * 1.5f));
      return GESTURE_RIGHT_FLICK;
    }

  return GESTURE_NONE;
}

void gesture_on_frame(const imu_frame_t *frame, void *user)
{
  float confidence = 0.0f;
  int   type;
  int   idx;

  (void)user;

  /* FSM：冷却期丢弃输入，计时结束后清窗重填 */
  if (g_state == G_COOLDOWN)
    {
      if (frame->timestamp_ms >= g_cooldown_until)
        {
          g_state = G_IDLE;
          g_wlen  = 0;
        }
      else
        {
          return;
        }
    }

  /* 阶段一/二：滑入窗口（满窗后移位，40 帧 memmove 开销可忽略） */
  if (g_wlen < WINDOW_FRAMES)
    {
      idx = g_wlen++;
    }
  else
    {
      memmove(&g_gx[0],   &g_gx[1],   (WINDOW_FRAMES - 1) * sizeof(float));
      memmove(&g_gy[0],   &g_gy[1],   (WINDOW_FRAMES - 1) * sizeof(float));
      memmove(&g_gz[0],   &g_gz[1],   (WINDOW_FRAMES - 1) * sizeof(float));
      memmove(&g_amag[0], &g_amag[1], (WINDOW_FRAMES - 1) * sizeof(float));
      idx = WINDOW_FRAMES - 1;
    }

  g_gx[idx]   = frame->gyro_dps[0];
  g_gy[idx]   = frame->gyro_dps[1];
  g_gz[idx]   = frame->gyro_dps[2];
  g_amag[idx] = sqrtf(frame->accel_g[0] * frame->accel_g[0] +
                      frame->accel_g[1] * frame->accel_g[1] +
                      frame->accel_g[2] * frame->accel_g[2]);

  if (g_wlen < WINDOW_FRAMES)
    {
      return;
    }

  if (++g_since_eval < EVAL_STEP_FRAMES)
    {
      return;
    }

  g_since_eval = 0;

  type = evaluate_window(&confidence);
  if (type != GESTURE_NONE)
    {
      /* 阶段七：封装事件 → EventBus（订阅方为判定系统） */
      gesture_event_t ev;

      ev.type         = type;
      ev.timestamp_ms = frame->timestamp_ms;
      ev.confidence   = confidence;

      aura_event_publish(AURA_EVENT_GESTURE, &ev);

      g_state          = G_COOLDOWN;
      g_cooldown_until = frame->timestamp_ms + COOLDOWN_MS;
    }
}

int gesture_recognizer_init(void)
{
  gesture_recognizer_reset();
  return 0;
}

void gesture_recognizer_reset(void)
{
  g_wlen           = 0;
  g_since_eval     = 0;
  g_state          = G_IDLE;
  g_cooldown_until = 0;
}
