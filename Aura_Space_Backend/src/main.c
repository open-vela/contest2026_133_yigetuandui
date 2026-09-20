#include <stdio.h>
#include <stdlib.h>
#include "../inc/game_engine.h"

// 模拟一段后端的 JSON 谱面数据
const char* mock_json_beatmap = 
"{"
"    \"bpm\": 120,"
"    \"notes\": ["
"        { \"timestamp_ms\": 1000, \"type\": 0, \"track_id\": 1 }," // 1秒时：抬腕上抛 (type 0)[cite: 1]
"        { \"timestamp_ms\": 2000, \"type\": 1, \"track_id\": 1 }," // 2秒时：外旋左甩 (type 1)[cite: 1]
"        { \"timestamp_ms\": 3000, \"type\": 2, \"track_id\": 1 }"  // 3秒时：内旋右甩 (type 2)[cite: 1]
"    ]"
"}";

int main() {
    printf("=== Aura-Space BeatFlicks 游戏引擎本地 Mock 测试开始 ===\n\n");

    // 1. 初始化/重置计分系统
    ResetScoreSystem();

    // 2. 解析谱面[cite: 1]
    Beatmap* map = ParseBeatmapJSON(mock_json_beatmap);
    if (map == NULL) {
        printf("错误：谱面解析失败！\n");
        return -1;
    }
    printf("谱面加载成功！BPM: %d, 总音符数: %d\n\n", map->bpm, map->total_notes);
    printf("----- 游戏模拟进行中 -----\n");

    // 3. 模拟主循环 (时间推移)
    // 假设系统以 10ms 每帧的速度运行，总共跑 4000ms (4秒)
    for (uint32_t current_time = 0; current_time <= 4000; current_time += 10) {
        
        // --- 模拟底层硬件中断产生的手势事件 ---

        // 场景 A：在 1020ms 时，用户做了一个完美的上抛动作 (距离1000ms误差仅20ms，在Perfect的100ms窗口内)[cite: 1]
        if (current_time == 1020) {
            printf("[时间 %4d ms] 接收到硬件输入：抬腕上抛 -> ", current_time);
            ProcessGestureEvent(current_time, NOTE_TYPE_UP_TOSS, map);
            printf("判定完成 (对应第一个音符)\n");
        }
        
        // 场景 B：在 2300ms 时，用户做了一个左甩动作 (距离2000ms误差300ms，超过100ms但在Good的500ms窗口内)[cite: 1]
        if (current_time == 2300) {
            printf("[时间 %4d ms] 接收到硬件输入：外旋左甩 -> ", current_time);
            ProcessGestureEvent(current_time, NOTE_TYPE_LEFT_FLICK, map);
            printf("判定完成 (对应第二个音符)\n");
        }
        
        // 场景 C：用户错过了第三个音符 (3000ms 的右甩)，没有任何硬件输入，交由超时检测处理
        
        // --- 每帧必须执行的超时检测 ---
        // 监控是否有音符错过了最晚的 Good 窗口 (500ms)[cite: 1]
        uint32_t pre_index = map->current_index;
        CheckTimeoutMiss(current_time, map);
        if (map->current_index > pre_index) {
             printf("[时间 %4d ms] 系统检测：发生超时 Miss！连击已断。\n", current_time);
        }
    }

    // 4. 游戏结束，生成结算面板[cite: 1]
    printf("\n=== 游戏结束，结算面板 ===\n");
    GameResultEvent final_result = GenerateGameResult();
    
    printf("最终总分: %d\n", final_result.final_score);
    printf("最大连击: %d\n", final_result.max_combo);
    printf("总准确率: %.2f%%\n", final_result.accuracy * 100.0f);
    printf("最终评级: %c\n", final_result.grade);

    // 5. 释放内存，防止泄露[cite: 1]
    if (map != NULL) {
        if (map->notes != NULL) {
            free(map->notes);
        }
        free(map);
    }

    printf("\n测试结束，引擎正常退出。\n");
    return 0;
}