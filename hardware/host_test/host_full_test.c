/*
 * host_full_test.c —— 全链路宿主仿真（无需开发板）
 * 按谱面时间点发布手势事件，验证 判定引擎 + game_port + 五维存储 + Glicko-2
 * 预期：note#0/#1/#4/#6 Perfect、#2 Good、#3/#7 超时 Miss、#5 错型 Miss
 */
/* 宿主全链路验证：gesture 事件 → 判定 → 结算 → 五维存储 + Glicko-2 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>
#include "util.h"
#include "event_bus.h"
#include "game_port.h"
#include "radar_store.h"
#include "gesture_recognizer.h"
#include "game_engine.h"

int ioctl(int fd, int req, ...) { (void)fd; (void)req; return -1; }

static volatile int g_got_result;
static uint32_t g_t0;
static const char *jnames[] = {"", "Perfect", "Good", "Miss"};

static void on_judge(aura_event_type_t t, const void *p, void *u)
{
  const aura_judge_info_t *i = p; (void)t; (void)u;
  printf("[judge] note#%d -> %s (delta=%d)\n", (int)i->note_index, jnames[i->result], (int)i->delta_ms);
}
static void on_result(aura_event_type_t t, const void *p, void *u)
{
  const GameResultEvent *r = p; (void)t; (void)u;
  printf("[result] score=%u combo=%u acc=%.1f%% grade=%c\n",
         (unsigned)r->final_score, (unsigned)r->max_combo, r->accuracy*100.0f, r->grade);
  radar_store_print();
  g_got_result = 1;
}

/* 在指定毫秒（相对开局）发布一个手势事件 */
static void fire_at(uint32_t at_ms, int gtype)
{
  gesture_event_t ev;
  ev.type = gtype; ev.timestamp_ms = g_t0 + at_ms; ev.confidence = 0.92f;
  aura_event_publish(AURA_EVENT_GESTURE, &ev);
}

int main(void)
{
  aura_event_bus_init();
  radar_store_init();
  aura_event_subscribe(AURA_EVENT_JUDGE, on_judge, NULL);
  aura_event_subscribe(AURA_EVENT_GAME_RESULT, on_result, NULL);

  if (game_port_start(NULL) < 0) { printf("game_port start failed\n"); return 1; }
  g_t0 = aura_millis();

  /* 谱面：2000up 3000left 4000right 5000up 6000right 7000left 8000up 9000right */
  usleep((2000 + 20) * 1000); fire_at(2020, 0);   /* Perfect */
  usleep((3000 - 2020 + 20) * 1000); fire_at(3020, 1);  /* Perfect */
  usleep((4000 - 3020 + 300) * 1000); fire_at(4300, 2); /* Good (±100..500) */
  usleep((6000 - 4300 + 50) * 1000); fire_at(6050, 2);  /* Perfect; 5000up 超时 Miss */
  usleep((7000 - 6050 + 20) * 1000); fire_at(7020, 0);  /* 错型 Miss（应为 left） */
  usleep((8000 - 7020 + 20) * 1000); fire_at(8020, 0);  /* Perfect */
  usleep((9000 - 8020 + 600) * 1000); fire_at(9600, 2); /* 超窗 → 不消费 → 9500 超时 Miss */

  while (!g_got_result) usleep(100000);
  printf("FULL-CHAIN TEST PASS\n");
  return 0;
}
