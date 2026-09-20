/****************************************************************************
 * aura_hw/game_port.c
 *
 * 打通链路：gesture_event(EventBus) → ProcessGestureEvent/CheckTimeoutMiss
 *           → AURA_EVENT_JUDGE（振动反馈）→ AURA_EVENT_GAME_RESULT
 *           → radar_store（五维聚合）+ Glicko-2（下一局密度档位）
 *
 * 说明：后端引擎 ProcessGestureEvent 不回报判定结果，这里用
 * "记录调用前 current_index → 调用后比较" 的包装方式取回结果，
 * 不修改后端代码（后端代码的所有权在牛睿涵）。
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include "util.h"
#include "event_bus.h"
#include "gesture_recognizer.h"
#include "vib_motor.h"
#include "radar_store.h"
#include "glicko2.h"
#include "game_port.h"

#include "game_engine.h"

/* 内置演示谱面：120BPM，8 个音符，间隔 1s。
 * 正式谱面由后端 FastAPI / 谱面生成器下发 JSON。
 */
static const char *s_demo_beatmap =
  "{ \"bpm\": 120, \"notes\": ["
  "{\"timestamp_ms\": 2000, \"type\": 0, \"track_id\": 1},"
  "{\"timestamp_ms\": 3000, \"type\": 1, \"track_id\": 1},"
  "{\"timestamp_ms\": 4000, \"type\": 2, \"track_id\": 1},"
  "{\"timestamp_ms\": 5000, \"type\": 0, \"track_id\": 1},"
  "{\"timestamp_ms\": 6000, \"type\": 2, \"track_id\": 1},"
  "{\"timestamp_ms\": 7000, \"type\": 1, \"track_id\": 1},"
  "{\"timestamp_ms\": 8000, \"type\": 0, \"track_id\": 1},"
  "{\"timestamp_ms\": 9000, \"type\": 2, \"track_id\": 1}"
  "]}";

static Beatmap       *g_map;
static pthread_t      g_tick_thread;
static volatile bool  g_running;
static volatile bool  g_tick_started;   /* 线程确实创建成功才可 join */
static glicko2_t      g_player;
static uint32_t       g_round_start_ms;

/* 判定 → 振动反馈映射（方案 3.3 模式表） */
static void judge_vib_feedback(int result)
{
  switch (result)
    {
      case JUDGE_PERFECT:
        vib_request(VIB_PATTERN_LONG_STRONG, VIB_PRIO_GAME);
        break;
      case JUDGE_GOOD:
        vib_request(VIB_PATTERN_DOUBLE_SHORT, VIB_PRIO_GAME);
        break;
      case JUDGE_MISS:
        vib_request(VIB_PATTERN_SINGLE_SHORT, VIB_PRIO_GAME);
        break;
      default:
        break;
    }
}

static void emit_judge(uint32_t idx, int32_t delta_ms)
{
  aura_judge_info_t info;

  info.note_index = (int32_t)idx;
  info.result     = (int32_t)g_map->notes[idx].result;
  info.delta_ms   = delta_ms;

  aura_event_publish(AURA_EVENT_JUDGE, &info);
  judge_vib_feedback(info.result);
}

/* 手势事件 → 判定（EventBus 订阅回调） */
static void on_gesture(aura_event_type_t type, const void *payload,
                       void *user)
{
  const gesture_event_t *ev = payload;
  uint32_t idx;
  int32_t  delta;
  uint32_t rel_time;

  (void)type;
  (void)user;

  if (g_map == NULL || g_map->current_index >= g_map->total_notes)
    {
      return;
    }

  /* 置信度过滤（0.7→0.5：阈值下调前弱手势大量被此门限丢弃） */
  if (ev->confidence < 0.5f)
    {
      return;
    }

  /* 谱面时间戳是本局相对时间，需扣除开局时刻（方案 3.4 时间同步） */
  rel_time = ev->timestamp_ms - g_round_start_ms;

  idx   = g_map->current_index;
  delta = (int32_t)rel_time - (int32_t)g_map->notes[idx].timestamp_ms;

  ProcessGestureEvent(rel_time, (NoteType)ev->type, g_map);

  if (g_map->current_index > idx)
    {
      emit_judge(idx, delta);   /* 音符被消费（命中或错型 Miss） */
    }
}

static void finish_round(void)
{
  GameResultEvent r;
  float density_rating;

  r = GenerateGameResult();

  /* 先聚合落盘，再广播结算事件：保证订阅方（AI 看板/数据层）读到的是最新五维 */
  radar_store_apply_game_result(&r, g_map->total_notes);
  aura_event_publish(AURA_EVENT_GAME_RESULT, &r);

  /* Glicko-2 更新玩家评分 → 下一局谱面密度档位（方案 3.1 第五节） */
  density_rating = 1500.0f + (glicko2_density_level(&g_player) - 3) * 200.0f;
  glicko2_update(&g_player, density_rating, 100.0f, r.accuracy);

  printf("[game] round done: score=%u max_combo=%u acc=%.1f%% grade=%c"
         " | glicko2 mu=%.0f next_density=%d\n",
         (unsigned)r.final_score, (unsigned)r.max_combo,
         r.accuracy * 100.0f, r.grade,
         g_player.rating, glicko2_density_level(&g_player));

  /* 演示模式：2 秒后自动重开一局（正式版由 UI 路由触发）。
   * 分段睡眠：退出模块时 game_port_stop 的 join 最多等 100ms，
   * 避免 UI 线程被 2s 睡眠卡住（"返回像卡死"的根因之一）。
   */
  for (int i = 0; i < 20 && g_running; i++)
    {
      usleep(100000);
    }

  if (!g_running)
    {
      return;   /* 正在退出：不重开，tick 线程随 g_running 退出 */
    }
  free(g_map->notes);
  free(g_map);
  ResetScoreSystem();

  g_map = ParseBeatmapJSON(s_demo_beatmap);
  if (g_map == NULL)
    {
      g_running = false;
      return;
    }

  g_round_start_ms = aura_millis();
  aura_event_publish(AURA_EVENT_ROUND_START, &g_round_start_ms);
  printf("[game] new round started\n");
}

/* 10ms 周期巡检：超时 Miss 检测（方案 3.1：主循环高频调用） */
static void *tick_loop(void *arg)
{
  (void)arg;

  while (g_running)
    {
      uint32_t idx;

      usleep(10000);

      if (g_map == NULL)
        {
          continue;
        }

      idx = g_map->current_index;
      CheckTimeoutMiss(aura_millis() - g_round_start_ms, g_map);

      if (g_map->current_index > idx)
        {
          emit_judge(idx, 0);
        }

      if (g_map->current_index >= g_map->total_notes)
        {
          finish_round();
        }
    }

  return NULL;
}

int game_port_start(const char *beatmap_json)
{
  glicko2_init(&g_player);
  ResetScoreSystem();

  g_map = ParseBeatmapJSON(beatmap_json ? beatmap_json : s_demo_beatmap);
  if (g_map == NULL)
    {
      printf("[game] beatmap parse failed\n");
      return -1;
    }

  g_round_start_ms = aura_millis();
  aura_event_publish(AURA_EVENT_ROUND_START, &g_round_start_ms);
  printf("[game] beatmap loaded: bpm=%u notes=%u\n",
         g_map->bpm, (unsigned)g_map->total_notes);

  aura_event_subscribe(AURA_EVENT_GESTURE, on_gesture, NULL);

  g_running = true;
  if (pthread_create(&g_tick_thread, NULL, tick_loop, NULL) != 0)
    {
      printf("[game] tick thread create failed\n");
      g_tick_started = false;
      return -1;
    }

  g_tick_started = true;
  return 0;
}

void game_port_stop(void)
{
  g_running = false;

  if (g_tick_started)
    {
      pthread_join(g_tick_thread, NULL);
      g_tick_started = false;
    }

  if (g_map)
    {
      free(g_map->notes);
      free(g_map);
      g_map = NULL;
    }
}
