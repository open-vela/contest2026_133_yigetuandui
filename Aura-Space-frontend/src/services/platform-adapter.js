import { FocusEngine } from "./focus-engine.js";
const delay = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

const dashboardFixture = {
  user: { name: "训练者", streakDays: 7, goal: "exam" },
  metrics: {
    reactionMs: 286,
    focusMinutes: 31,
    memoryAccuracy: 68,
    rhythmAccuracy: 88,
    flowMinutes: 18
  },
  baseline: { reactionMs: 305, focusMinutes: 28, flowMinutes: 16 },
  weekly: [62, 68, 64, 73, 77, 75, 82],
  latestSession: { module: "BeatFlicks", accuracy: 88, combo: 42, durationMinutes: 6 }
};

const chartFixture = {
  bpm: 118,
  durationMs: 16000,
  notes: [
    [1100, "up"], [1850, "left"], [2500, "right"], [3250, "up"],
    [3900, "right"], [4550, "left"], [5200, "up"], [5850, "left"],
    [6500, "right"], [7150, "up"], [7800, "up"], [8450, "left"],
    [9100, "right"], [9750, "left"], [10400, "up"], [11050, "right"],
    [11700, "up"], [12350, "left"], [13000, "right"], [13650, "up"],
    [14300, "left"], [14950, "right"]
  ].map(([timestamp, type], id) => ({ id, timestamp, type, lane: { left: 0, up: 1, right: 2 }[type] }))
};

function localCoachAnswer(question, analysis) {
  const text = question.trim();
  if (/记忆|N-?Back/i.test(text)) {
    return "你当前的工作记忆维度仍有提升空间。建议先完成 2 组 2‑Back：每组 3 分钟、组间休息 60 秒；正确率连续两组超过 75% 再提升 N 值。";
  }
  if (/反应|音游|Beat/i.test(text)) {
    return "你的节奏准确率不错。下一轮先用低密度谱面热身 90 秒，再把密度提高一档；保持动作幅度一致，优先守住 Perfect/Good 的稳定性。";
  }
  if (/疲劳|压力|休息|焦虑/i.test(text)) {
    return "先暂停高强度训练。跟随 Aura‑Focus 做 4 分钟呼吸：吸气 4 秒、呼气 6 秒；结束后若主观疲劳仍高，就把今天的目标改为恢复而不是突破。";
  }
  if (/今天|安排|计划/i.test(text)) {
    return `今天建议按“呼吸 4 分钟 → N‑Back 2 组 → BeatFlicks 3 分钟”训练。${analysis.suggestion}`;
  }
  return `我结合你的五维数据做了分析：${analysis.suggestion} 你也可以继续问我“今天怎么练”或“如何提升记忆”。`;
}

export class PlatformAdapter {
  constructor() {
    this.mode = localStorage.getItem("aura:data-mode") || "mock";
    // 默认baseUrl：不带/api，UI输入框填写 http://127.0.0.1:8000
    this.baseUrl = this.sanitizeBaseUrl(localStorage.getItem("aura:api-url") || "http://127.0.0.1:8000");
    this.focusEngine = new FocusEngine();
    this.records = JSON.parse(localStorage.getItem("aura:records") || "[]");
    console.log("[PlatformAdapter] init mode=", this.mode, "baseUrl=", this.baseUrl);
  }

  // 清洗函数：移除方括号、空格、末尾斜杠
  sanitizeBaseUrl(raw) {
    if (!raw) return "";
    let s = raw.replace(/[\[\]\s]/g, ""); // 删除 [] 和所有空格
    s = s.replace(/\/+$/, ""); // 删除末尾斜杠
    return s;
  }

  configure({ mode, baseUrl }) {
    if (mode) this.mode = mode;
    if (baseUrl) this.baseUrl = this.sanitizeBaseUrl(baseUrl);
    localStorage.setItem("aura:data-mode", this.mode);
    localStorage.setItem("aura:api-url", this.baseUrl);
    console.log("[PlatformAdapter] configure mode=", this.mode, "baseUrl=", this.baseUrl);
  }

  async request(path, options = {}) {
    const url = `${this.baseUrl}${path}`;
    console.log("[PlatformAdapter] request url=", url);
    const response = await fetch(url, {
      headers: { "Content-Type": "application/json", ...(options.headers || {}) },
      ...options
    });
    if (!response.ok) throw new Error(`API ${response.status} url:${url}`);
    return response.json();
  }

  async getDashboard() {
    if (this.mode === "api") {
      // 对齐后端：GET /api/dashboard
      const raw = await this.request("/api/dashboard");
      // 把后端返回 {radar,recommendation} 映射成前端dashboardFixture格式
      return {
        user: { name: "训练者", streakDays: 7, goal: "exam" },
        metrics: {
          reactionMs: raw.radar.reaction,
          focusMinutes: raw.radar.focus,
          memoryAccuracy: raw.radar.memory,
          rhythmAccuracy: raw.radar.pressure,
          flowMinutes: raw.radar.flow
        },
        baseline: { reactionMs: 305, focusMinutes: 28, flowMinutes: 16 },
        weekly: [62, 68, 64, 73, 77, 75, 82],
        latestSession: { module: "BeatFlicks", accuracy: 88, combo: 42, durationMinutes: 6 },
        recommendation: raw.recommendation
      };
    }
    await delay(180);
    return structuredClone(dashboardFixture);
  }

  async getBeatChart() {
    if (this.mode === "api") {
      // 后端 /api/beatmap 返回 {bpm, notes:[{timestamp_ms,type,track_id}]}
      const raw = await this.request("/api/beatmap");
      // 后端note type：0 up;1 left;2 right；映射前端type字符串
      const typeMap = {0:"up",1:"left",2:"right"};
      const notes = raw.notes.map((n, idx)=>{
        const t = typeMap[n.type];
        return {
          id: idx,
          timestamp: n.timestamp_ms,
          type: t,
          lane: { left:0, up:1, right:2 }[t]
        };
      })
      return {
        bpm: raw.bpm,
        durationMs: 16000,
        notes
      }
    }
    await delay(80);
    return structuredClone(chartFixture);
  }

  async submitTrainingRecord(record) {
    if (this.mode === "api") {
      // 后端POST /api/game_result 接收结算数据
      return this.request("/api/game_result", { method: "POST", body: JSON.stringify(record) });
    }
    this.records.unshift({ ...record, id: crypto.randomUUID?.() || `${Date.now()}`, createdAt: new Date().toISOString() });
    this.records = this.records.slice(0, 50);
    localStorage.setItem("aura:records", JSON.stringify(this.records));
    return { ok: true };
  }

  async askCoach(question, dashboard) {
    // 当前后端没有 /ai/coach，API模式下降级走本地规则（不发网络请求）
    await delay(550);
    const analysis = this.focusEngine.analyze(dashboard.metrics, dashboard.baseline, dashboard.user.goal);
    return { answer: localCoachAnswer(question, analysis), source: "local‑rule‑engine" };
  }
}

export class BackgroundSampler {
  constructor(onChange) {
    this.onChange = onChange;
    this.battery = 76;
    this.mode = "active";
    this.handleVisibility = () => this.recalculate();
  }
  start() {
    document.addEventListener("visibilitychange", this.handleVisibility);
    this.recalculate();
  }
  stop() {
    document.removeEventListener("visibilitychange", this.handleVisibility);
  }
  setBattery(value) {
    this.battery = Math.max(1, Math.min(100, Number(value)));
    this.recalculate();
  }
  recalculate(training = false) {
    if (this.battery < 20) this.mode = "battery-saver";
    else if (training) this.mode = "training";
    else if (document.hidden) this.mode = "background";
    else this.mode = "active";
    const modes = {
      training: { rate: "200Hz", power: "12mA", label: "训练" },
      active: { rate: "50Hz", power: "3mA", label: "亮屏" },
      background: { rate: "10Hz", power: "0.8mA", label: "后台" },
      "battery-saver": { rate: "1Hz/30s", power: "0.1mA", label: "省电" }
    };
    this.onChange?.({ ...modes[this.mode], mode: this.mode, battery: this.battery });
  }
}
