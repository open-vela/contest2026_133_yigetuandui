const COLORS = { up: "#ffbf47", left: "#55dfaa", right: "#48baf7" };
const SYMBOLS = { up: "↑", left: "↖", right: "↗" };

export class BeatFlicksRenderer {
  constructor(canvas, chart, callbacks = {}) {
    this.canvas = canvas;
    this.ctx = canvas.getContext("2d");
    this.chart = chart;
    this.callbacks = callbacks;
    this.pool = Array.from({ length: 32 }, () => ({ active: false }));
    this.nextNote = 0;
    this.score = 0;
    this.combo = 0;
    this.maxCombo = 0;
    this.hits = 0;
    this.total = 0;
    this.running = false;
    this.feedback = null;
    this.resizeObserver = new ResizeObserver(() => this.resize());
    this.resizeObserver.observe(canvas);
    this.resize();
  }

  resize() {
    const rect = this.canvas.getBoundingClientRect();
    const ratio = Math.min(window.devicePixelRatio || 1, 2);
    this.canvas.width = Math.max(1, Math.floor(rect.width * ratio));
    this.canvas.height = Math.max(1, Math.floor(rect.height * ratio));
    this.ctx.setTransform(ratio, 0, 0, ratio, 0, 0);
    this.width = rect.width;
    this.height = rect.height;
  }

  start() {
    this.reset();
    this.running = true;
    this.startedAt = performance.now() + 600;
    this.frame = requestAnimationFrame((time) => this.loop(time));
  }

  reset() {
    this.pool.forEach((note) => { note.active = false; });
    this.nextNote = 0;
    this.score = 0;
    this.combo = 0;
    this.maxCombo = 0;
    this.hits = 0;
    this.total = 0;
    this.feedback = null;
    this.callbacks.onStats?.(this.stats());
  }

  stop() {
    this.running = false;
    cancelAnimationFrame(this.frame);
    this.resizeObserver.disconnect();
  }

  loop(now) {
    if (!this.running) return;
    let elapsed = now - this.startedAt;
    if (elapsed > this.chart.durationMs) {
      this.callbacks.onRound?.(this.stats());
      this.startedAt = now + 850;
      this.pool.forEach((note) => { note.active = false; });
      this.nextNote = 0;
      elapsed = -850;
    }
    this.spawn(elapsed);
    this.expire(elapsed);
    this.draw(elapsed, now);
    this.frame = requestAnimationFrame((time) => this.loop(time));
  }

  spawn(elapsed) {
    while (this.nextNote < this.chart.notes.length && this.chart.notes[this.nextNote].timestamp - elapsed < 2400) {
      const source = this.chart.notes[this.nextNote++];
      const item = this.pool.find((note) => !note.active);
      if (!item) break;
      Object.assign(item, source, { active: true, judged: false });
    }
  }

  expire(elapsed) {
    for (const note of this.pool) {
      if (note.active && !note.judged && elapsed - note.timestamp > 145) {
        note.judged = true;
        note.active = false;
        this.applyJudgement("Miss", note.type, elapsed - note.timestamp);
      }
    }
  }

  gesture(type) {
    if (!this.running) return;
    const elapsed = performance.now() - this.startedAt;
    const candidates = this.pool
      .filter((note) => note.active && !note.judged && note.type === type)
      .map((note) => ({ note, delta: Math.abs(elapsed - note.timestamp) }))
      .sort((a, b) => a.delta - b.delta);
    const best = candidates[0];
    if (!best || best.delta > 160) {
      this.applyJudgement("Miss", type, best?.delta ?? 999);
      return;
    }
    best.note.judged = true;
    best.note.active = false;
    this.applyJudgement(best.delta <= 70 ? "Perfect" : "Good", type, best.delta);
  }

  applyJudgement(result, type, delta) {
    this.total += 1;
    if (result === "Perfect") {
      this.combo += 1;
      this.hits += 1;
      this.score += Math.round(1000 * (1 + Math.floor(this.combo / 10) * 0.1));
    } else if (result === "Good") {
      this.combo += 1;
      this.hits += 0.7;
      this.score += Math.round(650 * (1 + Math.floor(this.combo / 10) * 0.1));
    } else {
      this.combo = 0;
    }
    this.maxCombo = Math.max(this.maxCombo, this.combo);
    this.feedback = { result, type, delta, at: performance.now() };
    this.callbacks.onJudgement?.(this.feedback);
    this.callbacks.onStats?.(this.stats());
  }

  stats() {
    return {
      score: this.score,
      combo: this.combo,
      maxCombo: this.maxCombo,
      accuracy: this.total ? Math.round((this.hits / this.total) * 100) : 100
    };
  }

  draw(elapsed, now) {
    const ctx = this.ctx;
    const w = this.width;
    const h = this.height;
    ctx.clearRect(0, 0, w, h);
    const gradient = ctx.createLinearGradient(0, 0, 0, h);
    gradient.addColorStop(0, "#07191b");
    gradient.addColorStop(1, "#020707");
    ctx.fillStyle = gradient;
    ctx.fillRect(0, 0, w, h);

    const laneWidth = w / 3;
    for (let lane = 0; lane < 3; lane += 1) {
      ctx.fillStyle = lane % 2 ? "rgba(255,255,255,.025)" : "rgba(255,255,255,.012)";
      ctx.fillRect(lane * laneWidth, 0, laneWidth, h);
      ctx.strokeStyle = "rgba(255,255,255,.07)";
      ctx.beginPath();
      ctx.moveTo(lane * laneWidth, 0);
      ctx.lineTo(lane * laneWidth, h);
      ctx.stroke();
    }

    const targetY = h - 46;
    ctx.strokeStyle = "rgba(255,255,255,.42)";
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.moveTo(10, targetY);
    ctx.lineTo(w - 10, targetY);
    ctx.stroke();

    for (const note of this.pool) {
      if (!note.active) continue;
      const timeToTarget = note.timestamp - elapsed;
      const y = targetY - (timeToTarget / 2400) * (targetY + 30);
      if (y < -30 || y > h + 30) continue;
      const x = note.lane * laneWidth + laneWidth / 2;
      ctx.shadowBlur = 18;
      ctx.shadowColor = COLORS[note.type];
      ctx.fillStyle = COLORS[note.type];
      ctx.beginPath();
      ctx.arc(x, y, 18, 0, Math.PI * 2);
      ctx.fill();
      ctx.shadowBlur = 0;
      ctx.fillStyle = "#031312";
      ctx.font = "700 18px system-ui";
      ctx.textAlign = "center";
      ctx.textBaseline = "middle";
      ctx.fillText(SYMBOLS[note.type], x, y + 1);
    }

    if (this.feedback && now - this.feedback.at < 520) {
      const age = (now - this.feedback.at) / 520;
      ctx.globalAlpha = 1 - age;
      ctx.fillStyle = this.feedback.result === "Perfect" ? "#ffd56b" : this.feedback.result === "Good" ? "#77c8ff" : "#ff8791";
      ctx.font = "800 25px system-ui";
      ctx.textAlign = "center";
      ctx.fillText(this.feedback.result, w / 2, h * 0.43 - age * 18);
      ctx.globalAlpha = 1;
    }
  }
}
