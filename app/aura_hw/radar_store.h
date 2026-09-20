/****************************************************************************
 * aura_hw/radar_store.h
 * 五维雷达数据模型 + KV 持久化（对应方案 3.4 第四节）
 ****************************************************************************/

#ifndef AURA_HW_RADAR_STORE_H
#define AURA_HW_RADAR_STORE_H

#include <stdint.h>

#include "game_engine.h"   /* GameResultEvent（复用后端引擎头文件） */

/* 五维雷达指标，每维归一化 0~100（方案 3.4） */
typedef struct
{
  uint8_t reaction;    /* 反应速度（BeatFlicks 命中率） */
  uint8_t focus;       /* 持续专注（连击保持率） */
  uint8_t memory;      /* 工作记忆（N-Back 正确率，预留） */
  uint8_t pressure;    /* 生理压力（心率趋势，预留） */
  uint8_t flow;        /* 心流维持（Aura-Focus，预留） */
} radar5_t;

int  radar_store_init(void);

/* 一局结算后聚合进当日五维（EMA 平滑），并落盘 */
int  radar_store_apply_game_result(const GameResultEvent *r,
                                   uint32_t total_notes);

int  radar_store_get(radar5_t *out);
int  radar_store_flush(void);
void radar_store_print(void);

/* Aura-Focus：心流时长（分钟）计入 flow 维度 */
int  radar_store_apply_focus(uint32_t minutes);
/* Synapse-N：一局正确率（0~100）计入 memory 维度 */
int  radar_store_apply_nback(float accuracy_pct);

#endif /* AURA_HW_RADAR_STORE_H */
