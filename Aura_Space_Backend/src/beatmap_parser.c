#include "../inc/game_engine.h"
#include "../inc/cJSON.h"  // 引入 cJSON 库[cite: 1]
#include <stdlib.h>
#include <stdio.h>

// 传入 JSON 字符串，解析并返回构造好的 Beatmap 结构体指针
Beatmap* ParseBeatmapJSON(const char* json_string) {
    if (json_string == NULL) {
        return NULL;
    }

    // 1. 将字符串解析为 cJSON 对象[cite: 1]
    cJSON* root = cJSON_Parse(json_string);
    if (root == NULL) {
        printf("Error: 解析 JSON 失败。\n");
        return NULL;
    }

    // 2. 分配 Beatmap 结构体内存
    Beatmap* map = (Beatmap*)malloc(sizeof(Beatmap));
    if (map == NULL) {
        cJSON_Delete(root);
        return NULL;
    }
    map->current_index = 0; // 初始化索引

    // 3. 解析 BPM[cite: 1]
    cJSON* bpm_item = cJSON_GetObjectItem(root, "bpm");
    if (bpm_item != NULL && cJSON_IsNumber(bpm_item)) {
        map->bpm = (uint16_t)bpm_item->valueint;
    } else {
        map->bpm = 120; // 默认回退值
    }

    // 4. 解析音符数组 (notes)
    cJSON* notes_array = cJSON_GetObjectItem(root, "notes");
    if (notes_array != NULL && cJSON_IsArray(notes_array)) {
        // 获取音符总数[cite: 1]
        map->total_notes = (uint32_t)cJSON_GetArraySize(notes_array);
        
        // 5. 核心内存管理：为音符数组一次性分配连续内存，避免产生碎片[cite: 1]
        if (map->total_notes > 0) {
            map->notes = (NoteObject*)malloc(sizeof(NoteObject) * map->total_notes);
            
            if (map->notes != NULL) {
                int i = 0;
                cJSON* note_item = NULL;
                
                // 遍历 JSON 数组，填充 C 结构体
                cJSON_ArrayForEach(note_item, notes_array) {
                    cJSON* time = cJSON_GetObjectItem(note_item, "timestamp_ms");
                    cJSON* type = cJSON_GetObjectItem(note_item, "type");
                    cJSON* track = cJSON_GetObjectItem(note_item, "track_id");

                    if (time && type && track) {
                        map->notes[i].timestamp_ms = (uint32_t)time->valuedouble; // 解析时间点（ms）[cite: 1]
                        map->notes[i].type = (NoteType)type->valueint;            // 解析音符类型[cite: 1]
                        map->notes[i].track_id = (uint8_t)track->valueint;
                        map->notes[i].is_active = true;                           // 初始状态均为待判定
                        map->notes[i].result = JUDGE_UNJUDGED;
                    }
                    i++;
                }
            }
        } else {
            map->notes = NULL;
        }
    } else {
        map->total_notes = 0;
        map->notes = NULL;
    }

    // 6. 释放 cJSON 对象占用的临时内存（只清理 JSON 树，保留我们提取出来的数据）
    cJSON_Delete(root);

    return map;
}