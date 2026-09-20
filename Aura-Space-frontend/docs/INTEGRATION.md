# Aura-Space 前后端与硬件对接契约 v1

## 1. 设计原则

- 前端只依赖 `PlatformAdapter`，不直接读取传感器或 KV 存储；
- 所有时间统一使用毫秒，事件携带单调时钟 `timestamp`；
- 手势、判定、训练记录通过 EventBus 解耦；
- 当前 Mock 数据可独立演示，真实数据字段保持一致；
- AI 云端不可用时，必须降级到本地规则引擎。

## 2. HTTP 接口

### `GET /api/dashboard`

```json
{
  "user": { "name": "训练者", "streakDays": 7, "goal": "exam" },
  "metrics": {
    "reactionMs": 286,
    "focusMinutes": 31,
    "memoryAccuracy": 68,
    "rhythmAccuracy": 88,
    "flowMinutes": 18
  },
  "baseline": { "reactionMs": 305, "focusMinutes": 28, "flowMinutes": 16 },
  "weekly": [62, 68, 64, 73, 77, 75, 82],
  "latestSession": { "module": "BeatFlicks", "accuracy": 88, "combo": 42, "durationMinutes": 6 }
}
```

### `GET /api/games/beatflicks/chart?difficulty=adaptive`

```json
{
  "bpm": 118,
  "durationMs": 16000,
  "notes": [
    { "id": 0, "timestamp": 1100, "type": "up", "lane": 1 },
    { "id": 1, "timestamp": 1850, "type": "left", "lane": 0 }
  ]
}
```

约束：

- `type` 只能是 `up | left | right`；
- `lane` 只能是 `0 | 1 | 2`；
- `timestamp` 相对本局开始时间递增；
- 正式 Perfect/Good/Miss 以牛睿涵游戏引擎返回为准，前端本地判定只用于 Mock。

### `POST /api/training/records`

```json
{
  "module": "BeatFlicks",
  "score": 12650,
  "accuracy": 88,
  "maxCombo": 42,
  "durationMs": 16000
}
```

成功响应：`{ "ok": true, "recordId": "..." }`

### `POST /api/ai/coach`

```json
{
  "question": "今天怎么安排训练？",
  "metrics": {
    "reactionMs": 286,
    "focusMinutes": 31,
    "memoryAccuracy": 68,
    "rhythmAccuracy": 88,
    "flowMinutes": 18
  }
}
```

响应：

```json
{
  "answer": "今天建议按呼吸、记忆、节奏三个阶段训练……",
  "source": "xiaomi-ai-agent"
}
```

## 3. EventBus 事件

### 硬件 → 前端：`gesture:detected`

```json
{
  "type": "left",
  "timestamp": 121234567,
  "confidence": 0.94,
  "source": "imu"
}
```

### 游戏引擎 → 前端：`game:judgement`

```json
{
  "noteId": 12,
  "result": "Perfect",
  "deltaMs": -18,
  "combo": 23,
  "score": 8650
}
```

### N-Back 引擎 → 前端：`nback:stimulus`

```json
{
  "round": 8,
  "n": 2,
  "position": 3,
  "color": "#55dfaa",
  "displayMs": 850
}
```

### 前端 → 振动服务：`haptic:request`

```json
{
  "pattern": "nback-position-error",
  "pulses": 2,
  "intensity": 45,
  "durationMs": 25
}
```

## 4. 真机适配清单

1. 将 `PlatformAdapter.getDashboard()` 接到 KV/BLE 聚合数据；
2. 将 `getBeatChart()` 接到牛睿涵谱面引擎；
3. 将 IMU `gesture:detected` 订阅映射到 `BeatFlicksRenderer.gesture(type)`；
4. 将 Web Vibration 调用替换为詹诗涵的 `vibrate(pattern, intensity, duration)`；
5. 将 `submitTrainingRecord()` 写入隆彦洁的数据层与同步队列；
6. 将 `askCoach()` 接到 openvela AI Agent，保留本地规则降级路径；
7. 在 SF32LB52 模拟器与真机分别验证帧率、内存、触控热区和功耗。
