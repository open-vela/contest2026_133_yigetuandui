/****************************************************************************
 * aura_hw/radar_store.c
 *
 * 持久化双后端：
 *   - CONFIG_KVDB：openvela KV 存储（property_set，JSON 值）
 *   - 否则降级为 /data/aura_radar.json 文件（需 defconfig 挂载 littlefs）
 * 正式版键名应按 RTC 日期生成 "radar_YYYYMMDD"（方案 3.4），
 * RTC 未同步前先用固定键 "radar_today"。
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if defined(CONFIG_KVDB)
#  include <kvdb.h>
#endif

#include "cJSON.h"
#include "radar_store.h"

#define RADAR_KEY      "radar_today"
#define RADAR_EMA_A    0.3f    /* 指数滑动平均：新样本权重 */

static radar5_t g_today;

static uint8_t ema(uint8_t old_v, float sample)
{
  float v = (1.0f - RADAR_EMA_A) * old_v + RADAR_EMA_A * sample;

  if (v < 0.0f)   v = 0.0f;
  if (v > 100.0f) v = 100.0f;
  return (uint8_t)(v + 0.5f);
}

int radar_store_init(void)
{
  /* 初始基线 60 分；可从 KV 读回历史值，此处从简 */
  g_today.reaction = 60;
  g_today.focus    = 60;
  g_today.memory   = 60;
  g_today.pressure = 60;
  g_today.flow     = 60;
  return 0;
}

int radar_store_apply_game_result(const GameResultEvent *r,
                                  uint32_t total_notes)
{
  float acc100;
  float combo_ratio;

  if (r == NULL)
    {
      return -1;
    }

  acc100      = r->accuracy * 100.0f;
  combo_ratio = total_notes ? (float)r->max_combo / (float)total_notes
                            : 0.0f;

  g_today.reaction = ema(g_today.reaction, acc100);
  g_today.focus    = ema(g_today.focus, combo_ratio * 100.0f);
  /* memory / pressure / flow 由 N-Back、心率、Aura-Focus 模块上报后更新 */

  return radar_store_flush();
}

int radar_store_get(radar5_t *out)
{
  if (out == NULL)
    {
      return -1;
    }

  *out = g_today;
  return 0;
}

int radar_store_apply_focus(uint32_t minutes)
{
  float sample = (float)minutes * 10.0f;   /* 4 分钟 → 40 分位 */

  if (sample > 100.0f)
    {
      sample = 100.0f;
    }

  g_today.flow = ema(g_today.flow, sample);
  return radar_store_flush();
}

int radar_store_apply_nback(float accuracy_pct)
{
  g_today.memory = ema(g_today.memory, accuracy_pct);
  return radar_store_flush();
}

int radar_store_flush(void)
{
  cJSON *root;
  char  *json;
  int    ret = 0;

  root = cJSON_CreateObject();
  if (root == NULL)
    {
      return -1;
    }

  cJSON_AddNumberToObject(root, "reaction", g_today.reaction);
  cJSON_AddNumberToObject(root, "focus",    g_today.focus);
  cJSON_AddNumberToObject(root, "memory",   g_today.memory);
  cJSON_AddNumberToObject(root, "pressure", g_today.pressure);
  cJSON_AddNumberToObject(root, "flow",     g_today.flow);

  json = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  if (json == NULL)
    {
      return -1;
    }

#if defined(CONFIG_KVDB)
  ret = property_set(RADAR_KEY, json);
#else
  {
    FILE *f = fopen("/data/aura_radar.json", "w");
    if (f)
      {
        fputs(json, f);
        fclose(f);
      }
    else
      {
        ret = -1;   /* 无文件系统时静默失败，运行态数据仍可用 */
      }
  }
#endif

  cJSON_free(json);
  return ret;
}

void radar_store_print(void)
{
  printf("[radar] reaction=%u focus=%u memory=%u pressure=%u flow=%u\n",
         g_today.reaction, g_today.focus, g_today.memory,
         g_today.pressure, g_today.flow);
}
