const COLORS = ["#55dfaa", "#48baf7", "#ffbf47", "#f378d5"];

function seededSequence(length = 40) {
  let seed = 20260814;
  return Array.from({ length }, (_, index) => {
    seed = (seed * 1664525 + 1013904223) % 4294967296;
    const position = Math.floor((seed / 4294967296) * 4);
    seed = (seed * 1664525 + 1013904223) % 4294967296;
    const color = Math.floor((seed / 4294967296) * COLORS.length);
    if (index > 2 && index % 4 === 0) return { position: index % 8 === 0 ? position : null, color: index % 8 === 0 ? null : color };
    return { position, color };
  }).map((item, index, values) => ({
    position: item.position ?? values[Math.max(0, index - 2)]?.position ?? 0,
    color: item.color ?? values[Math.max(0, index - 2)]?.color ?? 0
  }));
}

export class SynapseNController {
  constructor(root, callbacks = {}) {
    this.root = root;
    this.callbacks = callbacks;
    this.n = 2;
    this.index = -1;
    this.sequence = seededSequence();
    this.answers = 0;
    this.correct = 0;
    this.running = false;
  }

  start() {
    this.running = true;
    this.next();
  }

  stop() {
    this.running = false;
    clearTimeout(this.timer);
    clearTimeout(this.fadeTimer);
  }

  next() {
    if (!this.running) return;
    this.index += 1;
    if (this.index >= this.sequence.length) this.index = 0;
    const stimulus = this.sequence[this.index];
    const cells = [...this.root.querySelectorAll(".quadrant")];
    cells.forEach((cell) => cell.classList.remove("is-stimulus", "fade-out"));
    const cell = cells[stimulus.position];
    if (cell) {
      cell.style.setProperty("--stimulus-color", COLORS[stimulus.color]);
      requestAnimationFrame(() => cell.classList.add("is-stimulus"));
      this.fadeTimer = setTimeout(() => cell.classList.add("fade-out"), 650);
    }
    this.callbacks.onStimulus?.({ index: this.index + 1, n: this.n });
    this.timer = setTimeout(() => this.next(), 1850);
  }

  answer(kind) {
    if (this.index < this.n) {
      this.callbacks.onFeedback?.({ result: "warmup", message: `还需观察 ${this.n - this.index} 个刺激` });
      return;
    }
    const current = this.sequence[this.index];
    const previous = this.sequence[this.index - this.n];
    const positionMatch = current.position === previous.position;
    const colorMatch = current.color === previous.color;
    const expected = positionMatch && colorMatch ? "both" : positionMatch ? "position" : colorMatch ? "color" : "none";
    const isCorrect = kind === expected;
    this.answers += 1;
    if (isCorrect) this.correct += 1;
    const accuracy = Math.round((this.correct / this.answers) * 100);
    const errorType = positionMatch !== (kind === "position" || kind === "both") ? "位置判断" : "颜色判断";
    this.callbacks.onFeedback?.({
      result: isCorrect ? "correct" : "incorrect",
      message: isCorrect ? "判断正确 · 单次短震" : `${errorType}有误 · ${errorType === "位置判断" ? "两" : "三"}次短震`,
      accuracy
    });
    this.callbacks.onStats?.({ accuracy, correct: this.correct, answers: this.answers, n: this.n });
    if (this.answers >= 8 && this.answers % 4 === 0) {
      if (accuracy >= 85 && this.n < 4) this.n += 1;
      else if (accuracy < 55 && this.n > 1) this.n -= 1;
    }
  }
}
