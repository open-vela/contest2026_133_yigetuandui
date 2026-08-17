# Aura-Space 团队对接契约 v1

## 后端数据

正式整合时，前端期望以下统一状态。当前快应用使用同结构的本地 Mock 数据。

```json
{
  "metrics": {
    "reactionMs": 286,
    "focusMinutes": 31,
    "memoryAccuracy": 68,
    "rhythmAccuracy": 88,
    "flowMinutes": 18
  },
  "beat": {
    "bpm": 118,
    "target": "left",
    "score": 12650,
    "combo": 42
  },
  "nback": {
    "n": 2,
    "position": 3,
    "color": "#55dfaa",
    "accuracy": 75
  }
}
```

## 硬件 → 前端

### IMU 手势事件

```json
{
  "event": "gesture:detected",
  "type": "left",
  "timestamp": 121234567,
  "confidence": 0.94,
  "source": "imu"
}
```

`type` 仅允许 `left | up | right`。前端将其映射到 BeatFlicks 的 `hitLeft / hitUp / hitRight`。

### 振动请求

```json
{
  "event": "haptic:request",
  "pattern": "nback-correct",
  "pulses": 1,
  "intensity": 45,
  "durationMs": 25
}
```

- N-Back 正确：1 次短震；
- N-Back 错误：2 次短震；
- BeatFlicks Miss：2 次短震；
- Perfect/Good：1 次短震。

## 前后端责任边界

- 前端不直接实现正式谱面生成和最终成绩结算；
- 后端/硬件只通过契约提供数据和事件，不直接操作页面节点；
- AI 服务不可用时，前端使用本地规则建议，确保可独立演示；
- 所有时间均使用毫秒，实时事件使用单调时钟。
