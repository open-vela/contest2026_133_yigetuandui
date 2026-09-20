/****************************************************************************
 * aura_hw/music.c
 * 节拍音乐合成器：底鼓(kick)/踩镲(hat)/音符重音(accent)/判定音
 * 16kHz 单声道 s16 PCM，软件合成，无版权素材依赖。
 ****************************************************************************/

#include <nuttx/config.h>

#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "util.h"
#include "music.h"
#include "game_engine.h"   /* JUDGE_* */

#define SAMPLE_RATE     16000
#define CHUNK_FRAMES    256        /* 每次写 16ms 数据 */

/* 一个短音效的描述 */
typedef struct
{
  uint32_t start_ms;      /* 相对音轨时间 */
  uint8_t  kind;          /* 0 kick 1 hat 2 tone 3 thud */
  float    freq;          /* tone 用 */
} sfx_event_t;

#define SFX_MAX 24

static int      g_audio_fd = -1;
static bool     g_running;
static uint16_t g_bpm = 120;
static uint32_t g_track_start;        /* 音轨起点（系统 ms） */
static uint32_t g_next_beat_ms;       /* 下一拍时刻 */
static uint32_t g_beat_count;
static uint32_t g_beat_pulse;         /* 视觉降级脉冲计数 */

static sfx_event_t g_sfx[SFX_MAX];
static int      g_sfx_head;
static int      g_sfx_tail;

static uint32_t g_lfsr = 0xace1u;

static float frand(void)
{
  g_lfsr = g_lfsr * 1664525u + 1013904223u;
  return ((g_lfsr >> 8) & 0xffff) / 65536.0f - 1.0f;   /* ±1 */
}

/****************************************************************************
 * 事件队列
 ****************************************************************************/

static void sfx_push(uint8_t kind, float freq, uint32_t delay_ms)
{
  int next = (g_sfx_tail + 1) % SFX_MAX;

  if (next == g_sfx_head)
    {
      return;   /* 满则丢弃（音效可丢失，不可阻塞） */
    }

  g_sfx[g_sfx_tail].kind     = kind;
  g_sfx[g_sfx_tail].freq     = freq;
  g_sfx[g_sfx_tail].start_ms = aura_millis() + delay_ms;
  g_sfx_tail = next;
}

/****************************************************************************
 * 合成单个样本（t 为音效触发后的秒数）
 ****************************************************************************/

static float synth_sample(uint8_t kind, float freq, float t)
{
  float v = 0.0f;

  switch (kind)
    {
      case 0:   /* kick：120→45Hz 指数扫频正弦，120ms 衰减 */
        if (t < 0.120f)
          {
            float f = 45.0f + 75.0f * expf(-t * 30.0f);
            v = sinf(2.0f * 3.14159265f * f * t) * expf(-t * 22.0f);
          }
        break;

      case 1:   /* hat：高通噪声 35ms */
        if (t < 0.035f)
          {
            v = frand() * expf(-t * 90.0f) * 0.6f;
          }
        break;

      case 2:   /* tone：方波音符 90ms（音符重音/提示音） */
        if (t < 0.090f)
          {
            float s = sinf(2.0f * 3.14159265f * freq * t);
            v = (s > 0.0f ? 0.5f : -0.5f) * expf(-t * 25.0f);
          }
        break;

      case 3:   /* thud：Miss 低频闷响 150ms */
        if (t < 0.150f)
          {
            v = sinf(2.0f * 3.14159265f * 70.0f * t) * expf(-t * 18.0f);
          }
        break;

      default:
        break;
    }

  return v;
}

/****************************************************************************
 * PCM 渲染与输出
 ****************************************************************************/

static void audio_render_write(uint32_t now_ms)
{
  static int16_t pcm[CHUNK_FRAMES];
  int i;
  int e;

  if (g_audio_fd < 0)
    {
      return;
    }

  for (i = 0; i < CHUNK_FRAMES; i++)
    {
      uint32_t t_ms = now_ms + (i * 1000) / SAMPLE_RATE;
      float    mix  = 0.0f;
      int      h    = g_sfx_head;

      while (h != g_sfx_tail)
        {
          sfx_event_t *ev = &g_sfx[h];
          float        t  = (float)((int32_t)t_ms - (int32_t)ev->start_ms)
                            / 1000.0f;

          if (t >= 0.0f)
            {
              mix += synth_sample(ev->kind, ev->freq, t);
            }

          h = (h + 1) % SFX_MAX;
        }

      if (mix > 1.0f)  mix = 1.0f;
      if (mix < -1.0f) mix = -1.0f;
      pcm[i] = (int16_t)(mix * 30000.0f);
    }

  /* 丢弃已结束的音效 */
  while (g_sfx_head != g_sfx_tail)
    {
      sfx_event_t *ev = &g_sfx[g_sfx_head];

      if ((int32_t)(now_ms - ev->start_ms) > 300)
        {
          g_sfx_head = (g_sfx_head + 1) % SFX_MAX;
        }
      else
        {
          break;
        }
    }

  e = write(g_audio_fd, pcm, sizeof(pcm));
  (void)e;   /* 音频设备被占用/异常时静默忽略，下帧重试 */
}

/****************************************************************************
 * 对外接口
 ****************************************************************************/

int music_init(void)
{
  g_audio_fd = open("/dev/audio0", O_WRONLY | O_NONBLOCK);
  if (g_audio_fd < 0)
    {
      g_audio_fd = open("/dev/audio", O_WRONLY | O_NONBLOCK);
    }

  if (g_audio_fd < 0)
    {
      printf("[music] /dev/audio 不存在，音乐降级为视觉节拍\n");
      return -1;
    }

  printf("[music] audio device ready\n");
  return 0;
}

void music_deinit(void)
{
  if (g_audio_fd >= 0)
    {
      close(g_audio_fd);
      g_audio_fd = -1;
    }
}

void music_start(uint16_t bpm)
{
  g_bpm         = bpm > 0 ? bpm : 120;
  g_running     = true;
  g_track_start = aura_millis();
  g_next_beat_ms = g_track_start;
  g_beat_count  = 0;
  g_beat_pulse  = 0;
  g_sfx_head    = 0;
  g_sfx_tail    = 0;
}

void music_stop(void)
{
  g_running = false;
}

void music_note_hit(int note_type)
{
  static const float tone[3] = { 880.0f, 660.0f, 550.0f };

  if (note_type >= 0 && note_type <= 2)
    {
      sfx_push(2, tone[note_type], 0);
    }
}

void music_judge(int judge_result)
{
  switch (judge_result)
    {
      case JUDGE_PERFECT:
        sfx_push(0, 0.0f, 0);              /* kick */
        sfx_push(2, 1320.0f, 30);          /* 高亮泛音 */
        break;
      case JUDGE_GOOD:
        sfx_push(0, 0.0f, 0);
        break;
      case JUDGE_MISS:
        sfx_push(3, 0.0f, 0);              /* thud */
        break;
      default:
        break;
    }
}

void music_tick(uint32_t now_ms, uint32_t round_start_ms)
{
  uint32_t beat_ms = 60000u / g_bpm;

  (void)round_start_ms;

  if (!g_running)
    {
      return;
    }

  /* 节拍调度：第 1 拍 kick，其余 hat（电子四四拍） */
  while ((int32_t)(now_ms - g_next_beat_ms) >= 0)
    {
      if ((g_beat_count % 4) == 0)
        {
          sfx_push(0, 0.0f, 0);
        }
      else
        {
          sfx_push(1, 0.0f, 0);
        }

      g_beat_count++;
      g_beat_pulse++;
      g_next_beat_ms += beat_ms;
    }

  audio_render_write(now_ms);
}

bool music_audio_ok(void)
{
  return g_audio_fd >= 0;
}

uint32_t music_consume_beat_pulse(void)
{
  uint32_t n = g_beat_pulse;

  g_beat_pulse = 0;
  return n;
}
