/****************************************************************************
 * aura_hw/event_bus.c
 ****************************************************************************/

#include "event_bus.h"

#include <string.h>

#define AURA_MAX_SUBSCRIBERS  8   /* 编译期静态表，避免运行时动态分配 */

typedef struct
{
  aura_event_handler_t handler;
  void                *user_data;
} subscriber_t;

static subscriber_t g_subs[AURA_EVENT_MAX][AURA_MAX_SUBSCRIBERS];
static uint8_t      g_sub_count[AURA_EVENT_MAX];

int aura_event_bus_init(void)
{
  memset(g_subs, 0, sizeof(g_subs));
  memset(g_sub_count, 0, sizeof(g_sub_count));
  return 0;
}

int aura_event_subscribe(aura_event_type_t type,
                         aura_event_handler_t handler, void *user_data)
{
  if (type >= AURA_EVENT_MAX || handler == NULL)
    {
      return -1;
    }

  if (g_sub_count[type] >= AURA_MAX_SUBSCRIBERS)
    {
      return -2;
    }

  g_subs[type][g_sub_count[type]].handler   = handler;
  g_subs[type][g_sub_count[type]].user_data = user_data;
  g_sub_count[type]++;
  return 0;
}

int aura_event_publish(aura_event_type_t type, const void *payload)
{
  int i;

  if (type >= AURA_EVENT_MAX)
    {
      return -1;
    }

  /* 同步派发：手势 → 判定链路为 O(1) 处理，直接在发布线程内完成。
   * 注意：当前发布者均为线程上下文。若将来在 ISR 中发布（如硬件定时器
   * 直推），需先投递到无锁环形队列再唤醒工作线程消费（方案 3.4 设计）。
   */
  for (i = 0; i < g_sub_count[type]; i++)
    {
      g_subs[type][i].handler(type, payload, g_subs[type][i].user_data);
    }

  return 0;
}
