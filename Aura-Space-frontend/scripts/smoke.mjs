import assert from "node:assert/strict";
import { EventBus } from "../src/core/event-bus.js";
import { FocusEngine } from "../src/services/focus-engine.js";

const bus = new EventBus();
let received = 0;
const unsubscribe = bus.on("test", (value) => { received += value; });
bus.emit("test", 2);
unsubscribe();
bus.emit("test", 3);
assert.equal(received, 2, "EventBus 应支持订阅与取消订阅");

const engine = new FocusEngine("exam");
const result = engine.analyze(
  { reactionMs: 286, focusMinutes: 31, memoryAccuracy: 68, rhythmAccuracy: 88, flowMinutes: 18 },
  { reactionMs: 305, focusMinutes: 28, flowMinutes: 16 },
  "exam"
);
assert.ok(result.score >= 0 && result.score <= 100, "专注分数应位于 0-100");
assert.equal(Object.keys(result.normalized).length, 5, "应输出五维能力画像");
assert.ok(result.suggestion.includes("提升空间"), "建议应采用正向反馈结构");

console.log("Smoke tests passed: EventBus + FocusEngine");
