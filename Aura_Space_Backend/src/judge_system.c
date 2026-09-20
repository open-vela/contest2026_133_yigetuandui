#include "../inc/game_engine.h"
#include <stdlib.h>

// ==========================================
// 静态全局状态变量（用于本局游戏的计分追踪）
// ==========================================
static uint32_t total_score = 0;
static uint32_t current_combo = 0;
static uint32_t max_combo = 0;
static uint32_t max_possible_score = 0; // 理论最高分，用于计算准确率
static uint32_t total_judged_notes = 0; // 已判定的音符数量

// 重置计分系统（新局开始时调用）
void ResetScoreSystem(void) {
    total_score = 0;
    current_combo = 0;
    max_combo = 0;
    max_possible_score = 0;
    total_judged_notes = 0;
}

// ==========================================
// 计分与结算模块
// ==========================================

// 内部调用：根据判定结果更新分数和连击数[cite: 1]
void UpdateScoreAndCombo(JudgeResult result) {
    uint32_t base_score = 0;
    float multiplier = 1.0f;

    // 1. 处理基础分与断连逻辑[cite: 1]
    switch (result) {
        case JUDGE_PERFECT:
            base_score = 10;          // Perfect 基础分 10 分[cite: 1]
            current_combo++;
            break;
        case JUDGE_GOOD:
            base_score = 5;           // Good 基础分 5 分[cite: 1]
            current_combo++;
            break;
        case JUDGE_MISS:
            base_score = 0;           // Miss 基础分 0 分[cite: 1]
            current_combo = 0;        // 断连处理[cite: 1]
            break;
        default:
            return;
    }

    // 2. 更新最大连击数
    if (current_combo > max_combo) {
        max_combo = current_combo;
    }

    // 3. 计算连击加成：每满10连击，分数倍率增加10%（例如10连击×1.1，20连击×1.2）[cite: 1]
    multiplier = 1.0f + ((current_combo / 10) * 0.1f);

    // 4. 累加实际得分与理论最高分
    total_score += (uint32_t)(base_score * multiplier);
    
    // 理论最高分：假设该音符打出 Perfect，且按当时的连击数计算倍率
    float max_multiplier = 1.0f + (((max_combo + 1) / 10) * 0.1f);
    max_possible_score += (uint32_t)(10 * max_multiplier); 
    
    total_judged_notes++;
}

// 获取当前游戏结算数据[cite: 1]
GameResultEvent GenerateGameResult(void) {
    GameResultEvent result;
    result.final_score = total_score;
    result.max_combo = max_combo;
    
    // 计算总准确率
    if (max_possible_score > 0) {
        result.accuracy = (float)total_score / (float)max_possible_score;
    } else {
        result.accuracy = 0.0f;
    }

    // 基于总准确率划分 S/A/B/C/D 五档评级[cite: 1]
    if (result.accuracy >= 0.95f) {
        result.grade = 'S';
    } else if (result.accuracy >= 0.85f) {
        result.grade = 'A';
    } else if (result.accuracy >= 0.70f) {
        result.grade = 'B';
    } else if (result.accuracy >= 0.60f) {
        result.grade = 'C';
    } else {
        result.grade = 'D';
    }

    return result;
}

// ==========================================
// 判定系统模块
// ==========================================

// 接收底层手势事件并进行时间窗口滑动匹配[cite: 1]
void ProcessGestureEvent(uint32_t gesture_time, NoteType gesture_type, Beatmap* map) {
    if (map == NULL || map->current_index >= map->total_notes) {
        return; // 谱面不存在或已结束
    }
    
    // O(1) 查找当前待判定音符[cite: 1]
    NoteObject* target_note = &map->notes[map->current_index];
    
    if (!target_note->is_active) {
        return; 
    }

    // 计算手势时间与目标音符时间戳的差值[cite: 1]
    int32_t time_diff = (int32_t)gesture_time - (int32_t)target_note->timestamp_ms;
    uint32_t abs_diff = (uint32_t)abs(time_diff);

    // 判断是否落在最大的判定窗口（Good 窗口：±500ms）内[cite: 1]
    if (abs_diff <= WINDOW_GOOD) {
        
        // 1. 手势类型不匹配 -> 直接判定 Miss[cite: 1]
        if (gesture_type != target_note->type) {
            target_note->result = JUDGE_MISS;
        } 
        // 2. 时间差绝对值 ≤ Perfect 窗口 (100ms) -> Perfect[cite: 1]
        else if (abs_diff <= WINDOW_PERFECT) {
            target_note->result = JUDGE_PERFECT;
        } 
        // 3. 其他情况落在 Good 窗口内 -> Good[cite: 1]
        else {
            target_note->result = JUDGE_GOOD;
        }
        
        // 标记该音符已处理，索引移动到下一个
        target_note->is_active = false;
        map->current_index++;
        
        // 更新分数
        UpdateScoreAndCombo(target_note->result);
        
        // TODO: 这里可以调用 EventBus 将单次判定结果 (target_note->result) 广播出去
    }
}

// 处理超时未命中 (需要在系统主循环中高频调用)[cite: 1]
void CheckTimeoutMiss(uint32_t current_sys_time, Beatmap* map) {
    if (map == NULL || map->current_index >= map->total_notes) {
        return;
    }
    
    NoteObject* target_note = &map->notes[map->current_index];
    
    if (!target_note->is_active) {
        return;
    }
    
    // 判定条件：当前系统时间已经超过了该音符的最晚命中时间（触发时间 + Good窗口）[cite: 1]
    if (current_sys_time > (target_note->timestamp_ms + WINDOW_GOOD)) {
        target_note->result = JUDGE_MISS;
        target_note->is_active = false;
        map->current_index++;
        
        UpdateScoreAndCombo(JUDGE_MISS);
        
        // TODO: 调用 EventBus 广播 Miss 事件[cite: 1]
    }
}