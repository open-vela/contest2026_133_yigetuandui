#ifndef GAME_ENGINE_H
#define GAME_ENGINE_H

#include <stdint.h>
#include <stdbool.h>

// 判定窗口时间常量
#define WINDOW_PERFECT 100   // ±100ms 内判定为 Perfect[cite: 1]
#define WINDOW_GOOD    500   // ±500ms 内判定为 Good[cite: 1]

// 音符类型枚举：对应底层识别的三种手势[cite: 1]
typedef enum {
    NOTE_TYPE_UP_TOSS = 0,   // 抬腕上抛[cite: 1]
    NOTE_TYPE_LEFT_FLICK,    // 外旋左甩[cite: 1]
    NOTE_TYPE_RIGHT_FLICK    // 内旋右甩[cite: 1]
} NoteType;

// 判定结果枚举[cite: 1]
typedef enum {
    JUDGE_UNJUDGED = 0,      // 尚未判定
    JUDGE_PERFECT,           // 完美命中[cite: 1]
    JUDGE_GOOD,              // 普通命中[cite: 1]
    JUDGE_MISS               // 错过或失误[cite: 1]
} JudgeResult;

// 单个音符数据结构
typedef struct {
    uint32_t timestamp_ms;   // 触发时间点（毫秒级）[cite: 1]
    NoteType type;           // 音符类型[cite: 1]
    uint8_t track_id;        // 轨道编号（预留给UI多轨渲染）[cite: 1]
    bool is_active;          // 是否处于活跃/待判定状态
    JudgeResult result;      // 判定结果
} NoteObject;

// 谱面整体结构体
typedef struct {
    uint16_t bpm;            // 谱面 BPM (每分钟节拍数)[cite: 1]
    uint32_t total_notes;    // 音符总数[cite: 1]
    NoteObject* notes;       // 音符数组指针（后续在解析时动态分配或指向静态内存池）
    uint32_t current_index;  // 当前正在判定的音符索引，避免每次从头遍历，实现 O(1) 查找
} Beatmap;

// 游戏结算数据包结构体[cite: 1]
typedef struct {
    char grade;              // 评级 (S/A/B/C/D) 基于总准确率计算[cite: 1]
    uint32_t final_score;    // 最终总分
    uint32_t max_combo;      // 最大连击数
    float accuracy;          // 总准确率 (0.0 ~ 1.0)
} GameResultEvent;

// ==========================================
// 核心模块函数声明
// ==========================================

// 1. 谱面解析模块
// 传入 JSON 字符串，解析并返回构造好的 Beatmap 结构体指针
Beatmap* ParseBeatmapJSON(const char* json_string);

// 2. 判定系统模块[cite: 1]
// 接收底层手势事件并进行时间窗口滑动判定
void ProcessGestureEvent(uint32_t gesture_time, NoteType gesture_type, Beatmap* map);
// 处理超时未命中 (需在系统主循环中高频调用，检查是否有音符已错过 Good 窗口)
void CheckTimeoutMiss(uint32_t current_sys_time, Beatmap* map);

// 3. 计分与结算模块
// 内部调用：更新分数和连击数
void UpdateScoreAndCombo(JudgeResult result);
// 获取当前游戏结算数据
GameResultEvent GenerateGameResult(void);
// 重置计分系统（新局开始时调用）
void ResetScoreSystem(void);

#endif // GAME_ENGINE_H