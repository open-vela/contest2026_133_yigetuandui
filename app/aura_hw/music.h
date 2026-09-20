/****************************************************************************
 * aura_hw/music.h
 * BeatFlicks 节拍音乐合成器（PCM 16kHz 单声道）
 *
 * 输出后端可插拔：/dev/audio 存在时真实放音；不存在时静默并降级为
 * 视觉节拍脉冲（UI 边框闪烁）。音频驱动就绪后无需改动上层。
 ****************************************************************************/

#ifndef AURA_HW_MUSIC_H
#define AURA_HW_MUSIC_H

#include <stdint.h>
#include <stdbool.h>

/* 初始化（尝试打开 /dev/audio，失败自动降级） */
int  music_init(void);
void music_deinit(void);

/* 对局开始：按 BPM 启动节拍音轨 */
void music_start(uint16_t bpm);
void music_stop(void);

/* 事件音：音符到达判定线（按类型给不同音高）；判定结果音 */
void music_note_hit(int note_type);
void music_judge(int judge_result);   /* JUDGE_PERFECT/GOOD/MISS */

/* 驱动节拍调度与 PCM 输出（须在主线程周期调用，16ms 粒度） */
void music_tick(uint32_t now_ms, uint32_t round_start_ms);

/* 状态查询 */
bool     music_audio_ok(void);
/* 视觉降级：返回并清除"本拍脉冲"计数（>0 表示刚过去一个节拍） */
uint32_t music_consume_beat_pulse(void);

#endif /* AURA_HW_MUSIC_H */
