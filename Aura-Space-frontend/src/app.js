import { eventBus } from "./core/event-bus.js";
import { BeatFlicksRenderer } from "./modules/beatflicks.js";
import { SynapseNController } from "./modules/synapse-n.js";
import { FocusEngine } from "./services/focus-engine.js";
import { BackgroundSampler, PlatformAdapter } from "./services/platform-adapter.js";

const root = document.querySelector("#viewRoot");
const toast = document.querySelector("#toast");
const adapter = new PlatformAdapter();
const focusEngine = new FocusEngine();
let dashboard = null;
let cleanupCurrentView = () => {};
let toastTimer;

const sampler = new BackgroundSampler((status) => {
  document.querySelector("#sampleRate").textContent = status.rate;
  document.querySelector("#battery").textContent = `${status.battery}%`;
});

sampler.start();

function showToast(message) {
  clearTimeout(toastTimer);
  toast.textContent = message;
  toast.classList.add("is-visible");
  toastTimer = setTimeout(() => toast.classList.remove("is-visible"), 2100);
}

function updateClock() {
  document.querySelector("#clock").textContent = new Intl.DateTimeFormat("zh-CN", {
    hour: "2-digit", minute: "2-digit", hour12: false
  }).format(new Date());
}
updateClock();
setInterval(updateClock, 30000);

function setLoading(label = "正在同步训练数据") {
  root.innerHTML = `<div class="loading-state"><div>${label}</div></div>`;
}

function errorTemplate(error) {
  return `<section class="screen-page">
    <div class="screen-heading"><div><span class="eyebrow-light">CONNECTION ERROR</span><h2>暂时无法获取数据</h2></div></div>
    <div class="card" style="padding:18px;line-height:1.7;color:#a8b9b6;font-size:10px">
      <p>${error.message || "接口请求失败"}</p>
      <p>请检查左侧 API 地址，或切回 Mock 数据继续演示。</p>
      <button class="primary-button" id="fallbackMock">切回 Mock 数据</button>
    </div>
  </section>`;
}

async function ensureDashboard() {
  if (!dashboard) dashboard = await adapter.getDashboard();
  return dashboard;
}

function setActiveNav(view) {
  document.querySelectorAll(".bottom-nav [data-nav]").forEach((button) => {
    button.classList.toggle("is-active", button.dataset.nav === view);
  });
}

async function navigate(view) {
  cleanupCurrentView();
  cleanupCurrentView = () => {};
  sampler.recalculate(false);
  setActiveNav(view);
  root.scrollTop = 0;
  root.dataset.view = view;
  try {
    if (view === "home") await renderHome();
    else if (view === "beat") await renderBeat();
    else if (view === "nback") await renderNBack();
    else if (view === "focus") await renderFocus();
    else if (view === "coach") await renderCoach();
  } catch (error) {
    root.innerHTML = errorTemplate(error);
    document.querySelector("#fallbackMock")?.addEventListener("click", () => {
      adapter.configure({ mode: "mock" });
      dashboard = null;
      syncConnectionUi();
      navigate("home");
    });
  }
  root.focus({ preventScroll: true });
}

async function renderHome() {
  setLoading();
  const data = await ensureDashboard();
  const analysis = focusEngine.analyze(data.metrics, data.baseline, data.user.goal);
  const dayLabels = ["一", "二", "三", "四", "五", "六", "日"];
  root.innerHTML = `<section class="screen-page home-page">
    <div class="home-hero">
      <div class="hero-copy">
        <span class="eyebrow-light">DAY ${data.user.streakDays} · EXAM MODE</span>
        <h2>早上好。<br>今天也进入心流。</h2>
        <p>${analysis.suggestion.split("。")[0]}。</p>
      </div>
      <div class="score-ring" style="--score:${analysis.score}"><strong>${analysis.score}</strong><small>FOCUS SCORE</small></div>
    </div>
    <div class="metric-strip">
      <div class="metric card"><span>反应速度</span><strong>${data.metrics.reactionMs}<small> ms</small></strong><small>较基线更快</small></div>
      <div class="metric card"><span>记忆正确</span><strong>${data.metrics.memoryAccuracy}<small>%</small></strong><small>稳定训练中</small></div>
      <div class="metric card"><span>今日专注</span><strong>${data.metrics.focusMinutes}<small> min</small></strong><small>超过基线</small></div>
    </div>
    <div class="section-label"><span>开始训练</span><span>智能难度已开启</span></div>
    <div class="module-grid">
      <button class="module-card" data-open="beat" style="--module-color:var(--gold)"><i>♪</i><strong>BeatFlicks</strong><small>体感节奏 · 3 分钟</small></button>
      <button class="module-card" data-open="nback" style="--module-color:var(--pink)"><i>N</i><strong>Synapse-N</strong><small>工作记忆 · 2-Back</small></button>
      <button class="module-card" data-open="focus" style="--module-color:var(--teal)"><i>◉</i><strong>Aura-Focus</strong><small>呼吸引导 · 4 分钟</small></button>
      <button class="module-card" data-open="coach" style="--module-color:var(--blue)"><i>✦</i><strong>AI 教练</strong><small>五维画像 · 个性建议</small></button>
    </div>
    <div class="section-label"><span>本周专注趋势</span><span>+12% ↑</span></div>
    <div class="trend-card card"><div class="trend-bars">${data.weekly.map((value, index) => `<i class="trend-bar" style="--value:${value}"><span>${dayLabels[index]}</span></i>`).join("")}</div></div>
  </section>`;
  root.querySelectorAll("[data-open]").forEach((button) => button.addEventListener("click", () => navigate(button.dataset.open)));
}

async function renderBeat() {
  setLoading("加载自适应谱面");
  const chart = await adapter.getBeatChart();
  root.innerHTML = `<section class="screen-page beat-page">
    <div class="screen-heading">
      <div><span class="eyebrow-light">BEATFLICKS · ${chart.bpm} BPM</span><h2>体感节奏</h2><p>绝对时间驱动 · 32 对象池</p></div>
      <span class="tiny-chip">ADAPTIVE</span>
    </div>
    <div class="beat-stage">
      <canvas id="beatCanvas" aria-label="三轨音符画布"></canvas>
      <div class="beat-overlay">
        <div class="beat-stat"><span>SCORE</span><strong id="beatScore">000000</strong></div>
        <div class="judgement-pill" id="judgement">等待节拍</div>
        <div class="beat-stat align-right"><span>COMBO</span><strong id="beatCombo">0</strong></div>
      </div>
    </div>
    <div class="gesture-pad">
      <button data-gesture="left"><b>↖</b><small>左甩 · A</small></button>
      <button data-gesture="up"><b>↑</b><small>上抛 · W</small></button>
      <button data-gesture="right"><b>↗</b><small>右甩 · D</small></button>
    </div>
    <p class="beat-tip">真机接入后，这三个操作由 IMU GestureEvent 替代</p>
  </section>`;
  sampler.recalculate(true);
  const score = root.querySelector("#beatScore");
  const combo = root.querySelector("#beatCombo");
  const judgement = root.querySelector("#judgement");
  const renderer = new BeatFlicksRenderer(root.querySelector("#beatCanvas"), chart, {
    onStats(stats) {
      score.textContent = String(stats.score).padStart(6, "0");
      combo.textContent = stats.combo;
    },
    onJudgement(result) {
      judgement.textContent = `${result.result} · ${Math.round(result.delta)}ms`;
      if (navigator.vibrate) navigator.vibrate(result.result === "Miss" ? [25, 35, 25] : 18);
      eventBus.emit("game:judgement", result);
    },
    onRound(stats) {
      adapter.submitTrainingRecord({ module: "BeatFlicks", ...stats, durationMs: chart.durationMs });
      showToast(`本轮完成 · 准确率 ${stats.accuracy}%`);
    }
  });
  const handleGesture = (type) => renderer.gesture(type);
  root.querySelectorAll("[data-gesture]").forEach((button) => button.addEventListener("pointerdown", () => handleGesture(button.dataset.gesture)));
  const keyHandler = (event) => {
    const map = { a: "left", w: "up", d: "right", ArrowLeft: "left", ArrowUp: "up", ArrowRight: "right" };
    const gesture = map[event.key] || map[event.key.toLowerCase?.()];
    if (gesture) { event.preventDefault(); handleGesture(gesture); }
  };
  window.addEventListener("keydown", keyHandler);
  renderer.start();
  cleanupCurrentView = () => {
    renderer.stop();
    window.removeEventListener("keydown", keyHandler);
    sampler.recalculate(false);
  };
}

async function renderNBack() {
  root.innerHTML = `<section class="screen-page nback-page">
    <div class="screen-heading">
      <div><span class="eyebrow-light">SYNAPSE-N · DUAL CHANNEL</span><h2>记忆矩阵</h2><p>150ms 淡入 · 500ms 保持 · 200ms 淡出</p></div>
      <span class="tiny-chip" id="nLevel">2-BACK</span>
    </div>
    <div class="nback-board card" id="nbackBoard">
      <div class="quadrant"></div><div class="quadrant"></div><div class="quadrant"></div><div class="quadrant"></div>
    </div>
    <div class="nback-status card">
      <div><strong id="stimulusCount">观察刺激</strong><span>判断是否与 N 步前一致</span></div>
      <span class="accuracy-badge" id="nAccuracy">--%</span>
    </div>
    <div class="answer-grid">
      <button class="secondary-button" data-answer="position">位置相同</button>
      <button class="secondary-button" data-answer="color">颜色相同</button>
      <button class="secondary-button" data-answer="both">都相同</button>
      <button class="secondary-button" data-answer="none">都不同</button>
    </div>
    <div class="nback-feedback" id="nFeedback">先观察两个刺激，再开始判断</div>
  </section>`;
  sampler.recalculate(true);
  const feedback = root.querySelector("#nFeedback");
  const controller = new SynapseNController(root.querySelector("#nbackBoard"), {
    onStimulus(info) {
      root.querySelector("#stimulusCount").textContent = `刺激 ${info.index}`;
      root.querySelector("#nLevel").textContent = `${info.n}-BACK`;
    },
    onFeedback(info) {
      feedback.textContent = info.message;
      feedback.className = `nback-feedback ${info.result}`;
      if (navigator.vibrate && info.result !== "warmup") navigator.vibrate(info.result === "correct" ? 25 : [20,35,20]);
    },
    onStats(stats) {
      root.querySelector("#nAccuracy").textContent = `${stats.accuracy}%`;
      adapter.submitTrainingRecord({ module: "Synapse-N", ...stats });
    }
  });
  root.querySelectorAll("[data-answer]").forEach((button) => button.addEventListener("click", () => controller.answer(button.dataset.answer)));
  controller.start();
  cleanupCurrentView = () => { controller.stop(); sampler.recalculate(false); };
}

async function renderFocus() {
  root.innerHTML = `<section class="screen-page focus-page">
    <div class="screen-heading" style="text-align:left">
      <div><span class="eyebrow-light">AURA-FOCUS · 4/6 RHYTHM</span><h2>呼吸光环</h2><p>吸气 4 秒 · 呼气 6 秒</p></div>
      <span class="tiny-chip">LOW POWER</span>
    </div>
    <div class="breath-stage">
      <div class="breath-orbit"><div class="breath-core" id="breathCore"><strong id="breathPhase">吸气</strong></div></div>
    </div>
    <h3 class="breath-timer" id="breathTimer">04:00</h3>
    <p class="breath-copy">把注意力放在光环的扩张与收缩上</p>
    <div class="focus-actions"><button class="primary-button" id="toggleBreath">开始引导</button><button class="secondary-button" id="resetBreath">重置</button></div>
  </section>`;
  let remaining = 240;
  let active = false;
  let timer;
  let phaseTimer;
  const core = root.querySelector("#breathCore");
  const phase = root.querySelector("#breathPhase");
  const toggle = root.querySelector("#toggleBreath");
  const format = () => `${String(Math.floor(remaining / 60)).padStart(2,"0")}:${String(remaining % 60).padStart(2,"0")}`;
  const stopTimers = () => { clearInterval(timer); clearInterval(phaseTimer); };
  const startTimers = () => {
    sampler.recalculate(true);
    let cycle = 0;
    phase.textContent = "吸气";
    timer = setInterval(() => {
      remaining -= 1;
      root.querySelector("#breathTimer").textContent = format();
      if (remaining <= 0) {
        remaining = 0;
        active = false;
        stopTimers();
        toggle.textContent = "再次开始";
        showToast("呼吸训练完成 · 心流已恢复");
        adapter.submitTrainingRecord({ module: "Aura-Focus", durationSeconds: 240 });
      }
    }, 1000);
    phaseTimer = setInterval(() => { cycle = (cycle + 1) % 10; phase.textContent = cycle < 4 ? "吸气" : "呼气"; }, 1000);
  };
  toggle.addEventListener("click", () => {
    active = !active;
    core.classList.toggle("is-paused", !active);
    toggle.textContent = active ? "暂停" : "继续";
    if (active) startTimers(); else { stopTimers(); sampler.recalculate(false); }
  });
  root.querySelector("#resetBreath").addEventListener("click", () => {
    active = false; remaining = 240; stopTimers(); core.classList.add("is-paused");
    toggle.textContent = "开始引导"; root.querySelector("#breathTimer").textContent = format(); phase.textContent = "吸气";
    sampler.recalculate(false);
  });
  core.classList.add("is-paused");
  cleanupCurrentView = () => { stopTimers(); sampler.recalculate(false); };
}

function drawRadar(canvas, values) {
  const ratio = Math.min(devicePixelRatio || 1, 2);
  const size = 118;
  canvas.width = size * ratio;
  canvas.height = size * ratio;
  const ctx = canvas.getContext("2d");
  ctx.scale(ratio, ratio);
  const center = size / 2;
  const radius = 43;
  const labels = ["反应", "专注", "记忆", "准确", "心流"];
  const data = [values.reaction, values.focus, values.memory, values.accuracy, values.flow];
  const point = (index, scale = 1) => {
    const angle = -Math.PI / 2 + index * Math.PI * 2 / 5;
    return [center + Math.cos(angle) * radius * scale, center + Math.sin(angle) * radius * scale];
  };
  for (const scale of [.33, .66, 1]) {
    ctx.beginPath();
    for (let i = 0; i < 5; i += 1) {
      const [x, y] = point(i, scale);
      if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
    }
    ctx.closePath(); ctx.strokeStyle = "rgba(255,255,255,.11)"; ctx.stroke();
  }
  ctx.beginPath();
  data.forEach((value, index) => {
    const [x, y] = point(index, value / 100);
    if (index === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
  });
  ctx.closePath(); ctx.fillStyle = "rgba(85,223,170,.22)"; ctx.fill(); ctx.strokeStyle = "#55dfaa"; ctx.lineWidth = 1.5; ctx.stroke();
  ctx.font = "7px system-ui"; ctx.fillStyle = "#8fa4a1"; ctx.textAlign = "center"; ctx.textBaseline = "middle";
  labels.forEach((label, index) => { const [x,y] = point(index,1.23); ctx.fillText(label,x,y); });
}

async function renderCoach() {
  setLoading("生成五维能力画像");
  const data = await ensureDashboard();
  const analysis = focusEngine.analyze(data.metrics, data.baseline, data.user.goal);
  root.innerHTML = `<section class="screen-page coach-page">
    <div class="screen-heading"><div><span class="eyebrow-light">AI COACH · HYBRID ENGINE</span><h2>数据教练</h2><p>本地规则引擎 + 云端 Agent 接口</p></div><span class="tiny-chip">${adapter.mode === "mock" ? "LOCAL" : "CLOUD"}</span></div>
    <div class="coach-summary card"><canvas id="radar" class="radar-canvas"></canvas><div class="coach-score"><span>综合专注分</span><strong>${analysis.score}</strong><p>${analysis.suggestion}</p></div></div>
    <div class="chat-list" id="chatList"><div class="message ai">我已结合你的历史基线完成分析。想先了解今天怎么练，还是查看某个能力维度？</div></div>
    <div class="quick-prompts"><button data-question="今天怎么安排训练？">今天怎么练</button><button data-question="如何提升工作记忆？">提升记忆</button><button data-question="我有点疲劳怎么办？">疲劳恢复</button></div>
    <form class="chat-form" id="chatForm"><input id="chatInput" maxlength="80" placeholder="问问你的训练教练…" autocomplete="off"><button aria-label="发送">↑</button></form>
  </section>`;
  drawRadar(root.querySelector("#radar"), analysis.normalized);
  const chatList = root.querySelector("#chatList");
  const input = root.querySelector("#chatInput");
  const addMessage = (text, role) => {
    const node = document.createElement("div");
    node.className = `message ${role}`;
    node.textContent = text;
    chatList.append(node);
    root.scrollTo({ top: root.scrollHeight, behavior: "smooth" });
    return node;
  };
  const ask = async (question) => {
    if (!question.trim()) return;
    addMessage(question, "user");
    const pending = addMessage("正在结合五维数据分析…", "ai");
    try {
      const answer = await adapter.askCoach(question, data);
      pending.textContent = answer.answer;
      eventBus.emit("coach:answer", answer);
    } catch (error) {
      pending.textContent = `云端暂不可用：${error.message}。可以切换 Mock 模式体验本地降级路径。`;
    }
  };
  root.querySelector("#chatForm").addEventListener("submit", (event) => { event.preventDefault(); const question = input.value; input.value = ""; ask(question); });
  root.querySelectorAll("[data-question]").forEach((button) => button.addEventListener("click", () => ask(button.dataset.question)));
}

document.addEventListener("click", (event) => {
  const target = event.target.closest("[data-nav]");
  if (target) navigate(target.dataset.nav);
});

function syncConnectionUi() {
  document.querySelector("#sourceLabel").textContent = adapter.mode === "mock" ? "Mock" : "API";
  document.querySelector("#connectionChip").textContent = adapter.mode === "mock" ? "LOCAL" : "REMOTE";
  document.querySelector("#apiUrl").value = adapter.baseUrl;
  document.querySelectorAll("[data-mode]").forEach((button) => button.classList.toggle("is-active", button.dataset.mode === adapter.mode));
}

document.querySelectorAll("[data-mode]").forEach((button) => button.addEventListener("click", () => {
  adapter.configure({ mode: button.dataset.mode });
  syncConnectionUi();
}));
document.querySelector("#saveConnection").addEventListener("click", () => {
  adapter.configure({ baseUrl: document.querySelector("#apiUrl").value.trim() });
  dashboard = null;
  syncConnectionUi();
  showToast("连接设置已保存");
  navigate("home");
});

syncConnectionUi();
navigate("home");
