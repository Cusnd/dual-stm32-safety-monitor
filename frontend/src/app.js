import { LocalInsightProvider } from "./aiProvider.js";
import { buildAnalysisSnapshot } from "./analysis.js";
import { alarmLabel, parseSerialLine } from "./parser.js";
import { ReplaySerialSource } from "./replaySerial.js";
import { WebSerialSource } from "./serial.js";

const MAX_HISTORY = 80;
const STALE_AFTER_MS = 3000;

const strings = {
  "zh-CN": {
    title: "环境安全数据看板",
    subtitle: "MONITOR USART1 / CH340C",
    connect: "连接串口",
    disconnect: "断开",
    startSimulation: "开始模拟",
    stopSimulation: "停止模拟",
    connected: "串口已连接",
    replaying: "模拟串口",
    disconnected: "未连接",
    unsupported: "当前浏览器不支持 Web Serial",
    stale: "数据已超时",
    live: "实时",
    lastUpdate: "最后更新",
    noData: "等待数据",
    temp: "温度",
    humidity: "湿度",
    mq135: "MQ135",
    mq2: "MQ2",
    flame: "火焰",
    alarm: "告警",
    trend: "最近趋势",
    log: "串口事件",
    profile: "阈值档位",
    mute: "静音",
    flash: "Flash",
    yes: "是",
    no: "否",
    aiInsights: "AI 洞察",
    aiRisk: "风险等级",
    aiEvidence: "主要证据",
    aiTrends: "趋势判断",
    aiActions: "建议动作",
    aiChat: "用户对话",
    chatPlaceholder: "现在安全吗？",
    send: "发送",
    replayStarted: "模拟串口开始",
    replayStopped: "模拟串口停止",
    replayLoop: "模拟串口循环",
    streamStale: "串口数据断流",
  },
  en: {
    title: "Safety Monitor Dashboard",
    subtitle: "MONITOR USART1 / CH340C",
    connect: "Connect serial",
    disconnect: "Disconnect",
    startSimulation: "Start replay",
    stopSimulation: "Stop replay",
    connected: "Serial connected",
    replaying: "Replay serial",
    disconnected: "Disconnected",
    unsupported: "Web Serial is not supported",
    stale: "Data stale",
    live: "Live",
    lastUpdate: "Last update",
    noData: "Waiting",
    temp: "Temp",
    humidity: "Humidity",
    mq135: "MQ135",
    mq2: "MQ2",
    flame: "Flame",
    alarm: "Alarm",
    trend: "Recent trend",
    log: "Serial events",
    profile: "Profile",
    mute: "Mute",
    flash: "Flash",
    yes: "Yes",
    no: "No",
    aiInsights: "AI Insights",
    aiRisk: "Risk level",
    aiEvidence: "Evidence",
    aiTrends: "Trend",
    aiActions: "Action",
    aiChat: "User Chat",
    chatPlaceholder: "Is it safe now?",
    send: "Send",
    replayStarted: "Replay serial started",
    replayStopped: "Replay serial stopped",
    replayLoop: "Replay serial loop",
    streamStale: "Serial data stream stopped",
  },
};

let locale = "zh-CN";
let serialSource = null;
let replaySource = null;
let sourceMode = "idle";
let latest = null;
let stale = false;
let history = [];
let eventRows = [];
let chatMessages = [];

const aiProvider = new LocalInsightProvider();

const dom = {
  title: document.querySelector("[data-i18n='title']"),
  subtitle: document.querySelector("[data-i18n='subtitle']"),
  connectButton: document.querySelector("#connectButton"),
  disconnectButton: document.querySelector("#disconnectButton"),
  simulationButton: document.querySelector("#simulationButton"),
  languageButton: document.querySelector("#languageButton"),
  supportMessage: document.querySelector("#supportMessage"),
  connectionState: document.querySelector("#connectionState"),
  dataState: document.querySelector("#dataState"),
  lastUpdate: document.querySelector("#lastUpdate"),
  metricGrid: document.querySelector("#metricGrid"),
  detailGrid: document.querySelector("#detailGrid"),
  eventLog: document.querySelector("#eventLog"),
  chart: document.querySelector("#trendChart"),
  aiHeading: document.querySelector("[data-i18n='aiInsights']"),
  aiProviderBadge: document.querySelector("#aiProviderBadge"),
  aiRiskLabel: document.querySelector("[data-i18n='aiRisk']"),
  aiRiskValue: document.querySelector("#aiRiskValue"),
  aiSummary: document.querySelector("#aiSummary"),
  aiEvidenceHeading: document.querySelector("[data-i18n='aiEvidence']"),
  aiEvidence: document.querySelector("#aiEvidence"),
  aiTrendsHeading: document.querySelector("[data-i18n='aiTrends']"),
  aiTrends: document.querySelector("#aiTrends"),
  aiActionsHeading: document.querySelector("[data-i18n='aiActions']"),
  aiActions: document.querySelector("#aiActions"),
  chatHeading: document.querySelector("[data-i18n='aiChat']"),
  chatMessages: document.querySelector("#chatMessages"),
  chatForm: document.querySelector("#chatForm"),
  chatInput: document.querySelector("#chatInput"),
  chatSubmit: document.querySelector("#chatSubmit"),
};

const metricModel = [
  { key: "tempC", label: "temp", unit: "C", className: "metric-temp" },
  { key: "humidityPct", label: "humidity", unit: "%", className: "metric-humidity" },
  { key: "mq135Raw", label: "mq135", unit: "raw", className: "metric-air" },
  { key: "mq2Raw", label: "mq2", unit: "raw", className: "metric-smoke" },
  { key: "flame", label: "flame", unit: "", className: "metric-flame" },
  { key: "alarm", label: "alarm", unit: "", className: "metric-alarm" },
];

function t(key) {
  return strings[locale][key] ?? strings.en[key] ?? key;
}

function setText(node, value) {
  if (node) {
    node.textContent = value;
  }
}

function escapeHtml(value) {
  return String(value)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#39;");
}

function formatClock(timestamp = Date.now()) {
  return new Date(timestamp).toLocaleTimeString(locale, { hour12: false });
}

function currentSnapshot() {
  return buildAnalysisSnapshot({ latest, history, stale, locale });
}

function applyLanguage() {
  document.documentElement.lang = locale;
  setText(dom.title, t("title"));
  setText(dom.subtitle, t("subtitle"));
  setText(dom.connectButton.querySelector("[data-label]"), t("connect"));
  setText(dom.disconnectButton.querySelector("[data-label]"), t("disconnect"));
  setText(dom.simulationButton.querySelector("[data-label]"), sourceMode === "replay" ? t("stopSimulation") : t("startSimulation"));
  setText(document.querySelector("[data-i18n='trend']"), t("trend"));
  setText(document.querySelector("[data-i18n='log']"), t("log"));
  setText(dom.aiHeading, t("aiInsights"));
  setText(dom.aiRiskLabel, t("aiRisk"));
  setText(dom.aiEvidenceHeading, t("aiEvidence"));
  setText(dom.aiTrendsHeading, t("aiTrends"));
  setText(dom.aiActionsHeading, t("aiActions"));
  setText(dom.chatHeading, t("aiChat"));
  dom.chatInput.placeholder = t("chatPlaceholder");
  setText(dom.chatSubmit.querySelector("[data-label]"), t("send"));
  dom.languageButton.textContent = locale === "zh-CN" ? "EN" : "中文";
  render();
}

function addEvent(message, type = "info") {
  const time = formatClock();
  eventRows = [{ time, message, type }, ...eventRows].slice(0, 12);
  renderEvents();
}

function renderEvents() {
  dom.eventLog.innerHTML = eventRows
    .map((row) => `<li class="${escapeHtml(row.type)}"><time>${escapeHtml(row.time)}</time><span>${escapeHtml(row.message)}</span></li>`)
    .join("");
}

function ingestLine(line) {
  const result = parseSerialLine(line);
  if (result.kind === "ignored") {
    return;
  }
  if (result.kind === "error") {
    addEvent(result.error, "error");
    return;
  }

  latest = result.data;
  stale = false;
  history = [...history, latest].slice(-MAX_HISTORY);
  addEvent(`seq=${latest.seq} ${alarmLabel(latest.alarm, locale)}`, latest.alarm);
  render();
}

function valueFor(metric) {
  if (!latest) {
    return "--";
  }
  if (metric.key === "flame") {
    return latest.flame ? t("yes") : t("no");
  }
  if (metric.key === "alarm") {
    return alarmLabel(stale ? "node_lost" : latest.alarm, locale);
  }
  return latest[metric.key];
}

function renderMetrics() {
  dom.metricGrid.innerHTML = metricModel
    .map((metric) => {
      const value = valueFor(metric);
      const unit = latest && metric.unit ? `<span class="unit">${escapeHtml(metric.unit)}</span>` : "";
      return `
        <article class="metric ${escapeHtml(metric.className)}">
          <span>${escapeHtml(t(metric.label))}</span>
          <strong>${escapeHtml(value)}${unit}</strong>
        </article>
      `;
    })
    .join("");

  const details = [
    [t("profile"), latest ? latest.thresholdProfile : "--"],
    [t("mute"), latest ? (latest.mute ? t("yes") : t("no")) : "--"],
    [t("flash"), latest ? (latest.flashReady ? "OK" : "--") : "--"],
    ["SEQ", latest ? latest.seq : "--"],
    ["STATUS", latest ? `0x${latest.status.toString(16).padStart(2, "0").toUpperCase()}` : "--"],
    ["TICK", latest ? `${latest.tickMs} ms` : "--"],
  ];

  dom.detailGrid.innerHTML = details
    .map(([label, value]) => `<div><span>${escapeHtml(label)}</span><strong>${escapeHtml(value)}</strong></div>`)
    .join("");
}

function renderStatus() {
  const support = WebSerialSource.isSupported();
  dom.supportMessage.hidden = support;
  setText(dom.supportMessage, support ? "" : t("unsupported"));

  const connectionText = sourceMode === "serial" ? t("connected") :
    sourceMode === "replay" ? t("replaying") : t("disconnected");
  setText(dom.connectionState, connectionText);
  dom.connectionState.dataset.state = sourceMode === "idle" ? "disconnected" : "connected";

  const dataLabel = !latest ? t("noData") : stale ? t("stale") : t("live");
  setText(dom.dataState, dataLabel);
  dom.dataState.dataset.state = stale ? "stale" : latest ? "live" : "empty";

  const last = latest ? formatClock(latest.receivedAt) : "--";
  setText(dom.lastUpdate, `${t("lastUpdate")}: ${last}`);
  setText(dom.simulationButton.querySelector("[data-label]"), sourceMode === "replay" ? t("stopSimulation") : t("startSimulation"));
}

function renderList(node, items) {
  node.innerHTML = items.map((item) => `<li>${escapeHtml(item)}</li>`).join("");
}

function renderInsight() {
  const snapshot = currentSnapshot();
  const insight = aiProvider.analyze(snapshot, locale);

  setText(dom.aiProviderBadge, insight.provider);
  setText(dom.aiRiskValue, insight.title);
  dom.aiRiskValue.dataset.risk = insight.riskLevel;
  setText(dom.aiSummary, insight.summary);
  renderList(dom.aiEvidence, insight.reasons);
  renderList(dom.aiTrends, insight.trends);
  renderList(dom.aiActions, insight.recommendations);
}

function renderChat() {
  dom.chatMessages.innerHTML = chatMessages
    .map((message) => `
      <li class="${escapeHtml(message.role)}">
        <span>${message.role === "user" ? "You" : "AI"}</span>
        <p>${escapeHtml(message.content)}</p>
      </li>
    `)
    .join("");
  dom.chatMessages.scrollTop = dom.chatMessages.scrollHeight;
}

function drawChart() {
  const canvas = dom.chart;
  if (!canvas) {
    return;
  }
  const rect = canvas.getBoundingClientRect();
  const dpr = window.devicePixelRatio || 1;
  canvas.width = Math.max(1, Math.floor(rect.width * dpr));
  canvas.height = Math.max(1, Math.floor(rect.height * dpr));

  const ctx = canvas.getContext("2d");
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  ctx.clearRect(0, 0, rect.width, rect.height);

  const padding = { top: 18, right: 18, bottom: 26, left: 36 };
  const width = rect.width - padding.left - padding.right;
  const height = rect.height - padding.top - padding.bottom;

  ctx.strokeStyle = "#d8dde3";
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.moveTo(padding.left, padding.top);
  ctx.lineTo(padding.left, padding.top + height);
  ctx.lineTo(padding.left + width, padding.top + height);
  ctx.stroke();

  if (history.length < 2) {
    ctx.fillStyle = "#667085";
    ctx.font = "13px system-ui, sans-serif";
    ctx.fillText(t("noData"), padding.left + 8, padding.top + 26);
    return;
  }

  drawSeries(ctx, rect, padding, "tempC", "#0f766e", 0, 80);
  drawSeries(ctx, rect, padding, "humidityPct", "#2563eb", 0, 100);
  drawSeries(ctx, rect, padding, "mq2Raw", "#dc6803", 0, 4095);
  drawSeries(ctx, rect, padding, "mq135Raw", "#7c3aed", 0, 4095);

  ctx.font = "12px system-ui, sans-serif";
  drawLegend(ctx, padding.left + 8, padding.top + 4, "#0f766e", "T");
  drawLegend(ctx, padding.left + 54, padding.top + 4, "#2563eb", "H");
  drawLegend(ctx, padding.left + 102, padding.top + 4, "#dc6803", "MQ2");
  drawLegend(ctx, padding.left + 166, padding.top + 4, "#7c3aed", "MQ135");
}

function drawSeries(ctx, rect, padding, key, color, min, max) {
  const width = rect.width - padding.left - padding.right;
  const height = rect.height - padding.top - padding.bottom;

  ctx.strokeStyle = color;
  ctx.lineWidth = 2;
  ctx.beginPath();
  history.forEach((item, index) => {
    const x = padding.left + (index / Math.max(1, history.length - 1)) * width;
    const ratio = Math.max(0, Math.min(1, (item[key] - min) / (max - min)));
    const y = padding.top + height - ratio * height;
    if (index === 0) {
      ctx.moveTo(x, y);
    } else {
      ctx.lineTo(x, y);
    }
  });
  ctx.stroke();
}

function drawLegend(ctx, x, y, color, label) {
  ctx.fillStyle = color;
  ctx.fillRect(x, y, 16, 3);
  ctx.fillStyle = "#344054";
  ctx.fillText(label, x + 21, y + 5);
}

function render() {
  renderStatus();
  renderMetrics();
  renderInsight();
  renderEvents();
  renderChat();
  drawChart();
}

async function connectSerial() {
  try {
    await stopReplay();
    serialSource = new WebSerialSource({
      onLine: ingestLine,
      onStatus: (state) => {
        if (state === "connected") {
          sourceMode = "serial";
          addEvent(t("connected"));
        } else {
          if (sourceMode === "serial") {
            sourceMode = "idle";
          }
          addEvent(t("disconnected"));
        }
        render();
      },
      onError: (error) => addEvent(error.message ?? String(error), "error"),
    });
    await serialSource.connect();
  } catch (error) {
    addEvent(error.message ?? String(error), "error");
  }
}

async function disconnectSerial() {
  await serialSource?.disconnect();
  serialSource = null;
  if (sourceMode === "serial") {
    sourceMode = "idle";
  }
  render();
}

async function startReplay() {
  await disconnectSerial();
  replaySource = new ReplaySerialSource({
    onLine: ingestLine,
    onStatus: (state) => {
      if (state === "connected") {
        sourceMode = "replay";
        addEvent(t("replayStarted"));
      } else if (state === "loop") {
        addEvent(t("replayLoop"));
      } else {
        if (sourceMode === "replay") {
          sourceMode = "idle";
        }
        addEvent(t("replayStopped"));
      }
      render();
    },
    onError: (error) => addEvent(error.message ?? String(error), "error"),
    intervalMs: 700,
    loop: true,
  });
  await replaySource.connect();
}

async function stopReplay() {
  await replaySource?.disconnect();
  replaySource = null;
  if (sourceMode === "replay") {
    sourceMode = "idle";
  }
  render();
}

async function toggleReplay() {
  if (sourceMode === "replay") {
    await stopReplay();
  } else {
    await startReplay();
  }
}

async function handleChatSubmit(event) {
  event.preventDefault();
  const content = dom.chatInput.value.trim();
  if (!content) {
    return;
  }

  chatMessages = [...chatMessages, { role: "user", content }];
  dom.chatInput.value = "";
  renderChat();

  try {
    const reply = await aiProvider.chat({
      messages: chatMessages,
      snapshot: currentSnapshot(),
      locale,
    });
    chatMessages = [...chatMessages, reply].slice(-12);
  } catch (error) {
    chatMessages = [...chatMessages, { role: "assistant", content: error.message ?? String(error) }].slice(-12);
  }
  renderChat();
}

dom.connectButton.addEventListener("click", connectSerial);
dom.disconnectButton.addEventListener("click", disconnectSerial);
dom.simulationButton.addEventListener("click", toggleReplay);
dom.languageButton.addEventListener("click", () => {
  locale = locale === "zh-CN" ? "en" : "zh-CN";
  applyLanguage();
});
dom.chatForm.addEventListener("submit", handleChatSubmit);
window.addEventListener("resize", drawChart);

setInterval(() => {
  if (latest && !stale && Date.now() - latest.receivedAt > STALE_AFTER_MS) {
    stale = true;
    addEvent(t("streamStale"), "warn");
    render();
  }
}, 500);

applyLanguage();
