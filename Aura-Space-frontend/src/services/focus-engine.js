const clamp = (value, min = 0, max = 100) => Math.min(max, Math.max(min, value));

export const GOAL_WEIGHTS = {
  balanced: { reaction: 0.22, focus: 0.24, memory: 0.22, accuracy: 0.18, flow: 0.14 },
  reaction: { reaction: 0.4, focus: 0.18, memory: 0.15, accuracy: 0.17, flow: 0.1 },
  focus: { reaction: 0.15, focus: 0.4, memory: 0.15, accuracy: 0.15, flow: 0.15 },
  exam: { reaction: 0.12, focus: 0.22, memory: 0.45, accuracy: 0.13, flow: 0.08 }
};

export class FocusEngine {
  constructor(goal = "balanced") {
    this.goal = GOAL_WEIGHTS[goal] ? goal : "balanced";
  }

  normalize(metrics, baseline) {
    const reactionRatio = baseline.reactionMs / Math.max(metrics.reactionMs, 1);
    return {
      reaction: clamp(70 + (reactionRatio - 1) * 120),
      focus: clamp((metrics.focusMinutes / Math.max(baseline.focusMinutes, 1)) * 72),
      memory: clamp(metrics.memoryAccuracy),
      accuracy: clamp(metrics.rhythmAccuracy),
      flow: clamp(metrics.flowMinutes / Math.max(baseline.flowMinutes, 1) * 75)
    };
  }

  analyze(metrics, baseline, goal = this.goal) {
    const normalized = this.normalize(metrics, baseline);
    const weights = GOAL_WEIGHTS[goal] || GOAL_WEIGHTS.balanced;
    const score = Math.round(Object.entries(weights).reduce(
      (total, [key, weight]) => total + normalized[key] * weight,
      0
    ));
    const ranked = Object.entries(normalized).sort((a, b) => a[1] - b[1]);
    const weakest = ranked[0][0];
    const strongest = ranked.at(-1)[0];
    return { score, normalized, weakest, strongest, suggestion: this.suggest(normalized, weakest, strongest) };
  }

  suggest(values, weakest, strongest) {
    const labels = { reaction: "反应速度", focus: "持续专注", memory: "工作记忆", accuracy: "节奏准确", flow: "心流维持" };
    const actions = {
      reaction: "完成 1 组 3 分钟 BeatFlicks 热身，先保持准确再提高速度。",
      focus: "开始 4 分钟 Aura-Focus 呼吸，再进入一轮低密度节奏训练。",
      memory: "先做 2 组 2-Back，正确率稳定在 75% 后再升级。",
      accuracy: "把谱面密度降低一档，专注命中窗口与甩腕方向。",
      flow: "缩短单次训练、增加间隔，用 4-6 呼吸节奏重新进入状态。"
    };
    return `${labels[strongest]}表现最好（${Math.round(values[strongest])}分）。${labels[weakest]}还有提升空间：${actions[weakest]}`;
  }
}
