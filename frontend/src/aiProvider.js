import { thresholdForProfile } from "./analysis.js";

const phrases = {
  "zh-CN": {
    localProvider: "本地规则",
    deepSeekProvider: "DeepSeek",
    hybridProvider: "DeepSeek / 本地规则",
    fallbackProvider: "本地规则兜底",
    noData: "现在还没有有效数据。先启动模拟串口或连接板 B 串口，我再帮你分析。",
    stale: "不建议判断为安全。当前数据已经超时，先检查板间通信和板 B 串口输出。",
    safe: (snapshot) => `当前风险等级是「${snapshot.riskLabel}」。${snapshot.summary} 关键依据：${snapshot.reasons.join("；")}`,
    alarm: (snapshot) => `报警依据是：${snapshot.reasons.join("；")}。趋势：${snapshot.trends.join("；")}`,
    mq2: (latest, thresholds) => `MQ2 当前为 ${latest.mq2Raw}，预警阈值 ${thresholds.smokeWarn}，危险阈值 ${thresholds.smokeDanger}。${latest.mq2Raw >= thresholds.smokeDanger ? "已经进入危险区间。" : latest.mq2Raw >= thresholds.smokeWarn ? "已经进入预警区间。" : "暂未触发烟雾阈值。"}`,
    rain: (latest, thresholds) => `雨量 ADC 当前为 ${latest.rainRaw ?? "--"}，湿触发阈值 ${thresholds.rainWet}。${latest.rainWet ? "雨量模块已经触发湿态。" : "雨量模块暂未触发湿态。"}`,
    therm: (latest, thresholds) => `热敏温度当前为 ${Number.isFinite(latest.thermC10) ? (latest.thermC10 / 10).toFixed(1) : "--"}°C，预警阈值 ${(thresholds.thermWarnC10 / 10).toFixed(1)}°C，危险阈值 ${(thresholds.thermDangerC10 / 10).toFixed(1)}°C。${latest.thermHot ? "DO 高温已经触发。" : "DO 高温暂未触发。"}`,
    next: (snapshot) => `建议：${snapshot.recommendations.join("；")}`,
    default: (snapshot) => `我看到当前状态是「${snapshot.riskLabel}」。${snapshot.summary} 建议：${snapshot.recommendations.join("；")}`,
    disabled: "DeepSeek 后端代理未启用，已切换到本地规则。",
    emptyReply: "后端没有返回可显示的回复。",
  },
  en: {
    localProvider: "Local rules",
    deepSeekProvider: "DeepSeek",
    hybridProvider: "DeepSeek / Local rules",
    fallbackProvider: "Local fallback",
    noData: "No valid data is available yet. Start replay or connect Board B serial first.",
    stale: "I would not call it safe. The data is stale, so check board communication and Board B serial output.",
    safe: (snapshot) => `Current risk level is ${snapshot.riskLabel}. ${snapshot.summary} Evidence: ${snapshot.reasons.join("; ")}`,
    alarm: (snapshot) => `Alarm evidence: ${snapshot.reasons.join("; ")}. Trend: ${snapshot.trends.join("; ")}`,
    mq2: (latest, thresholds) => `MQ2 is ${latest.mq2Raw}; warning threshold is ${thresholds.smokeWarn}, danger threshold is ${thresholds.smokeDanger}. ${latest.mq2Raw >= thresholds.smokeDanger ? "It is in the danger range." : latest.mq2Raw >= thresholds.smokeWarn ? "It is in the warning range." : "It has not crossed the smoke threshold."}`,
    rain: (latest, thresholds) => `Rain ADC is ${latest.rainRaw ?? "--"}; wet threshold is ${thresholds.rainWet}. ${latest.rainWet ? "The rain module is wet-triggered." : "The rain module is not wet-triggered."}`,
    therm: (latest, thresholds) => `Thermistor is ${Number.isFinite(latest.thermC10) ? (latest.thermC10 / 10).toFixed(1) : "--"}°C; warning threshold is ${(thresholds.thermWarnC10 / 10).toFixed(1)}°C and danger threshold is ${(thresholds.thermDangerC10 / 10).toFixed(1)}°C. ${latest.thermHot ? "The DO high-temperature output is triggered." : "The DO high-temperature output is not triggered."}`,
    next: (snapshot) => `Recommendation: ${snapshot.recommendations.join("; ")}`,
    default: (snapshot) => `Current state is ${snapshot.riskLabel}. ${snapshot.summary} Recommendation: ${snapshot.recommendations.join("; ")}`,
    disabled: "DeepSeek proxy is disabled; using local rules.",
    emptyReply: "The backend returned no displayable reply.",
  },
};

function p(locale) {
  return phrases[locale] ?? phrases.en;
}

function lastUserText(messages) {
  const last = [...messages].reverse().find((message) => message.role === "user");
  return String(last?.content ?? "").toLowerCase();
}

function safeMessage(message) {
  return {
    role: message.role === "assistant" ? "assistant" : message.role === "system" ? "system" : "user",
    content: String(message.content ?? "").slice(0, 2000),
  };
}

function compactFrame(frame) {
  if (!frame) {
    return null;
  }

  return {
    seq: frame.seq,
    tickMs: frame.tickMs,
    tempC: frame.tempC,
    humidityPct: frame.humidityPct,
    mq135Raw: frame.mq135Raw,
    mq2Raw: frame.mq2Raw,
    rainRaw: frame.rainRaw,
    thermC10: frame.thermC10,
    flame: frame.flame,
    rainWet: frame.rainWet,
    thermHot: frame.thermHot,
    alarm: frame.alarm,
    status: frame.status,
    thresholdProfile: frame.thresholdProfile,
    mute: frame.mute,
    flashReady: frame.flashReady,
    flashRecords: frame.flashRecords,
    externalRgb: frame.externalRgb,
    schemaVersion: frame.schemaVersion,
    receivedAt: frame.receivedAt,
  };
}

function compactSnapshot(snapshot) {
  return {
    latest: compactFrame(snapshot.latest),
    history: (snapshot.history ?? []).slice(-12).map(compactFrame),
    stale: snapshot.stale,
    riskLevel: snapshot.riskLevel,
    riskLabel: snapshot.riskLabel,
    summary: snapshot.summary,
    reasons: snapshot.reasons,
    trends: snapshot.trends,
    recommendations: snapshot.recommendations,
    thresholds: snapshot.thresholds,
    generatedAt: snapshot.generatedAt,
    freshness: snapshot.freshness,
  };
}

function systemPrompt(locale) {
  if (locale === "zh-CN") {
    return [
      "你是双 STM32F103 环境安全监测系统的值守助手。",
      "只基于当前传感器快照和最近历史回答，不编造硬件状态。",
      "回答要短、明确，并优先说明风险等级、关键证据和下一步动作。",
    ].join("");
  }

  return [
    "You are the duty assistant for a dual STM32F103 environmental safety monitor.",
    "Answer only from the current sensor snapshot and recent history; do not invent hardware state.",
    "Keep replies concise and prioritize risk level, evidence, and next action.",
  ].join(" ");
}

function buildBackendMessages(messages, snapshot, locale) {
  return [
    { role: "system", content: systemPrompt(locale) },
    {
      role: "system",
      content: `Current sensor analysis snapshot:\n${JSON.stringify(compactSnapshot(snapshot), null, 2)}`,
    },
    ...messages.slice(-10).map(safeMessage),
  ];
}

function extractAssistantContent(payload) {
  if (!payload || typeof payload !== "object") {
    return typeof payload === "string" ? payload : "";
  }

  const direct = payload.content ?? payload.reply ?? payload.answer ?? payload.text;
  if (typeof direct === "string") {
    return direct;
  }

  const messageContent = payload.message?.content ?? payload.data?.message?.content ?? payload.data?.content;
  if (typeof messageContent === "string") {
    return messageContent;
  }

  const choiceContent = payload.choices?.[0]?.message?.content ?? payload.choices?.[0]?.delta?.content;
  return typeof choiceContent === "string" ? choiceContent : "";
}

function normalizeAssistantReply(payload, provider, locale) {
  if (payload?.role === "assistant" && typeof payload.content === "string") {
    return {
      role: "assistant",
      content: payload.content,
      provider: payload.provider ?? provider,
    };
  }

  const content = extractAssistantContent(payload).trim();
  return {
    role: "assistant",
    content: content || p(locale).emptyReply,
    provider: payload?.provider ?? provider,
  };
}

async function readResponsePayload(response) {
  const contentType = response.headers.get("Content-Type") ?? "";
  if (contentType.toLowerCase().includes("application/json")) {
    return response.json();
  }

  return response.text();
}

async function fetchJsonWithTimeout(url, options, timeoutMs) {
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), timeoutMs);

  try {
    return await fetch(url, { ...options, signal: controller.signal });
  } finally {
    clearTimeout(timeout);
  }
}

export class LocalInsightProvider {
  constructor() {
    this.id = "local-rules";
  }

  analyze(snapshot, locale = "zh-CN") {
    return {
      provider: p(locale).localProvider,
      riskLevel: snapshot.riskLevel,
      title: snapshot.riskLabel,
      summary: snapshot.summary,
      reasons: snapshot.reasons,
      trends: snapshot.trends,
      recommendations: snapshot.recommendations,
    };
  }

  async chat({ messages, snapshot, locale = "zh-CN" }) {
    const text = lastUserText(messages);
    const t = p(locale);

    if (!snapshot.latest) {
      return { role: "assistant", content: t.noData, provider: t.localProvider };
    }
    if (snapshot.stale || snapshot.riskLevel === "offline") {
      return { role: "assistant", content: t.stale, provider: t.localProvider };
    }

    if (text.includes("安全") || text.includes("safe")) {
      return { role: "assistant", content: t.safe(snapshot), provider: t.localProvider };
    }
    if (text.includes("报警") || text.includes("alarm") || text.includes("为什么") || text.includes("why")) {
      return { role: "assistant", content: t.alarm(snapshot), provider: t.localProvider };
    }
    if (text.includes("mq2") || text.includes("烟") || text.includes("smoke")) {
      return {
        role: "assistant",
        content: t.mq2(snapshot.latest, thresholdForProfile(snapshot.latest.thresholdProfile)),
        provider: t.localProvider,
      };
    }
    if (text.includes("雨") || text.includes("rain")) {
      return {
        role: "assistant",
        content: t.rain(snapshot.latest, thresholdForProfile(snapshot.latest.thresholdProfile)),
        provider: t.localProvider,
      };
    }
    if (text.includes("热敏") || text.includes("therm") || text.includes("ntc")) {
      return {
        role: "assistant",
        content: t.therm(snapshot.latest, thresholdForProfile(snapshot.latest.thresholdProfile)),
        provider: t.localProvider,
      };
    }
    if (text.includes("下一步") || text.includes("排查") || text.includes("next") || text.includes("check")) {
      return { role: "assistant", content: t.next(snapshot), provider: t.localProvider };
    }
    return { role: "assistant", content: t.default(snapshot), provider: t.localProvider };
  }
}

export class DeepSeekProvider {
  constructor({
    endpoint = "/api/ai/chat",
    model = "deepseek-v4-flash",
    enabled = true,
    timeoutMs = 12000,
    fallbackToLocal = true,
  } = {}) {
    this.id = "deepseek";
    this.endpoint = endpoint;
    this.model = model;
    this.enabled = enabled;
    this.timeoutMs = timeoutMs;
    this.fallbackToLocal = fallbackToLocal;
    this.localProvider = new LocalInsightProvider();
  }

  analyze(snapshot, locale = "zh-CN") {
    const insight = this.localProvider.analyze(snapshot, locale);
    return {
      ...insight,
      provider: this.enabled ? p(locale).hybridProvider : p(locale).localProvider,
    };
  }

  async chat({ messages, snapshot, locale = "zh-CN" }) {
    if (!this.enabled) {
      return this.localProvider.chat({ messages, snapshot, locale });
    }

    const requestBody = {
      model: this.model,
      messages: buildBackendMessages(messages, snapshot, locale),
      conversation: messages.slice(-10).map(safeMessage),
      snapshot: compactSnapshot(snapshot),
      locale,
      requestType: "chat",
    };

    try {
      const response = await fetchJsonWithTimeout(this.endpoint, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(requestBody),
      }, this.timeoutMs);

      if (!response.ok) {
        throw new Error(`DeepSeek proxy HTTP ${response.status}`);
      }

      const payload = await readResponsePayload(response);
      return normalizeAssistantReply(payload, p(locale).deepSeekProvider, locale);
    } catch (error) {
      if (!this.fallbackToLocal) {
        throw error;
      }

      const reply = await this.localProvider.chat({ messages, snapshot, locale });
      return {
        ...reply,
        provider: p(locale).fallbackProvider,
        fallbackReason: error.message ?? String(error),
      };
    }
  }
}
