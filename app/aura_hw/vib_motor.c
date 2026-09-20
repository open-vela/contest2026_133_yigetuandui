/****************************************************************************
 * aura_hw/vib_motor.c
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include <nuttx/timers/pwm.h>

#include "util.h"
#include "vib_motor.h"

#ifndef CONFIG_EXAMPLES_AURA_HW_PWM_PATH
#  define CONFIG_EXAMPLES_AURA_HW_PWM_PATH  "/dev/pwm0"
#endif

typedef struct
{
  uint16_t freq_hz;     /* ERM 马达谐振频率附近（典型 200~320Hz） */
  uint8_t  duty_pct;    /* 占空比 = 振动强度 */
  uint16_t on_ms;
  uint16_t off_ms;
} vib_step_t;

/* 方案 3.3 五种振动模式时序表 */
static const vib_step_t s_single[] =
{
  { 250, 60, 50, 0 }
};

static const vib_step_t s_double[] =
{
  { 250, 70, 50, 50 }, { 250, 70, 50, 0 }
};

static const vib_step_t s_triple[] =
{
  { 250, 80, 50, 50 }, { 250, 80, 50, 50 }, { 250, 80, 50, 0 }
};

static const vib_step_t s_long[] =
{
  { 250, 100, 100, 0 }
};

static const vib_step_t s_dual[] =
{
  { 320, 100, 50, 50 }, { 170, 40, 50, 0 }   /* 高频强 → 低频弱 */
};

typedef struct
{
  const vib_step_t *steps;
  uint8_t           count;
} vib_pattern_t;

static const vib_pattern_t s_patterns[] =
{
  { NULL, 0 },
  { s_single, 1 },
  { s_double, 2 },
  { s_triple, 3 },
  { s_long,   1 },
  { s_dual,   2 }
};

#define VIB_QUEUE_DEPTH  8   /* 方案 3.4：振动请求队列深度 8 */

typedef struct
{
  uint8_t pattern;
  uint8_t prio;
} vib_req_t;

static int              g_fd = -1;
static pthread_t        g_thread;
static pthread_mutex_t  g_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t   g_cond = PTHREAD_COND_INITIALIZER;
static vib_req_t        g_queue[VIB_QUEUE_DEPTH];
static int              g_qlen;
static volatile bool    g_running;
static volatile bool    g_abort_cur;     /* 高优先级抢占标志 */
static volatile int     g_cur_prio = -1;

static void pwm_set(uint16_t freq_hz, uint8_t duty_pct)
{
  struct pwm_info_s info;

  memset(&info, 0, sizeof(info));
  info.frequency = freq_hz;
  info.duty      = (ub16_t)(((uint32_t)duty_pct * 65535UL) / 100UL);

  ioctl(g_fd, PWMIOC_SETCHARACTERISTICS, (unsigned long)(uintptr_t)&info);
  ioctl(g_fd, PWMIOC_START, 0);
}

static void pwm_off(void)
{
  ioctl(g_fd, PWMIOC_STOP, 0);
}

/* 可被打断的毫秒睡眠：高优先级请求到达时提前返回 */
static bool sleep_abortable(uint32_t ms)
{
  uint32_t until = aura_millis() + ms;

  while (aura_millis() < until)
    {
      if (g_abort_cur)
        {
          return true;
        }

      usleep(1000);
    }

  return g_abort_cur;
}

static void *vib_worker(void *arg)
{
  (void)arg;

  while (g_running)
    {
      vib_req_t req;
      const vib_pattern_t *p;
      int i;

      pthread_mutex_lock(&g_lock);
      while (g_qlen == 0 && g_running)
        {
          pthread_cond_wait(&g_cond, &g_lock);
        }

      if (!g_running)
        {
          pthread_mutex_unlock(&g_lock);
          break;
        }

      req = g_queue[0];
      memmove(&g_queue[0], &g_queue[1],
              sizeof(vib_req_t) * (VIB_QUEUE_DEPTH - 1));
      g_qlen--;
      g_cur_prio  = req.prio;
      g_abort_cur = false;
      pthread_mutex_unlock(&g_lock);

      p = &s_patterns[req.pattern];
      for (i = 0; i < p->count; i++)
        {
          pwm_set(p->steps[i].freq_hz, p->steps[i].duty_pct);
          if (sleep_abortable(p->steps[i].on_ms))
            {
              break;
            }

          pwm_off();
          if (sleep_abortable(p->steps[i].off_ms))
            {
              break;
            }
        }

      pwm_off();
      g_cur_prio = -1;
    }

  return NULL;
}

int vib_motor_init(void)
{
  g_fd = open(CONFIG_EXAMPLES_AURA_HW_PWM_PATH, O_RDONLY);
  if (g_fd < 0)
    {
      /* 仿真环境无 PWM 节点时允许继续运行（振动静默失败） */
      printf("[vib] open %s failed, vibration disabled\n",
             CONFIG_EXAMPLES_AURA_HW_PWM_PATH);
      return -1;
    }

  g_running = true;
  return pthread_create(&g_thread, NULL, vib_worker, NULL);
}

int vib_request(vib_pattern_id_t pattern, vib_priority_t prio)
{
  int pos;

  if (g_fd < 0 || pattern <= 0 || pattern > VIB_PATTERN_DUAL_FREQ)
    {
      return -1;
    }

  pthread_mutex_lock(&g_lock);

  if (g_qlen >= VIB_QUEUE_DEPTH)
    {
      pthread_mutex_unlock(&g_lock);
      return -2;
    }

  /* 按优先级插入队首方向（SYSTEM > GAME > NBACK） */
  pos = g_qlen;
  while (pos > 0 && g_queue[pos - 1].prio < (uint8_t)prio)
    {
      g_queue[pos] = g_queue[pos - 1];
      pos--;
    }

  g_queue[pos].pattern = (uint8_t)pattern;
  g_queue[pos].prio    = (uint8_t)prio;
  g_qlen++;

  /* 正在播放的请求优先级更低 → 抢占打断 */
  if (g_cur_prio >= 0 && (int)prio > g_cur_prio)
    {
      g_abort_cur = true;
    }

  pthread_cond_signal(&g_cond);
  pthread_mutex_unlock(&g_lock);
  return 0;
}

void vib_stop_all(void)
{
  pthread_mutex_lock(&g_lock);
  g_qlen      = 0;
  g_abort_cur = true;
  pthread_mutex_unlock(&g_lock);
}

void vib_motor_deinit(void)
{
  g_running = false;
  pthread_mutex_lock(&g_lock);
  pthread_cond_signal(&g_cond);
  pthread_mutex_unlock(&g_lock);

  if (g_fd >= 0)
    {
      pthread_join(g_thread, NULL);
      pwm_off();
      close(g_fd);
      g_fd = -1;
    }
}
