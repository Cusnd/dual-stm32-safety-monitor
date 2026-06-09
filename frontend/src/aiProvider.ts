import { thresholdsForRecord } from "./analysis";
import type {
  AiProvider,
  AnalysisSnapshot,
  AssistantReply,
  BackendMessage,
  ChatMessage,
  Insight,
  Locale,
  SensorRecord,
  ThresholdProfile,
} from "./types";

const phrases = {
  "zh-CN": {
    localProvider: "本地规则",
    deepSeekProvider: "DeepSeek",
    directProvider: "DeepSeek 直连",
    proxyProvider: "DeepSeek 代理",
    hybridProvider: "DeepSeek 直连 / 本地规则",
    proxyHybridProvider: "DeepSeek 代理 / 本地规则",
    fallbackProvider: "本地规则兜底",
    noData: "现在还没有有效数据。先启动模拟串口或连接板 B 串口，我再帮你分析。",
    stale: "不建议判断为安全。当前数据已经超时，先检查板间通信和板 B 串口输出。",
    safe: (snapshot: AnalysisSnapshot) => `当前风险等级是「${snapshot.riskLabel}」。${snapshot.summary} 关键依据：${snapshot.reasons.join("；")}`,
    alarm: (snapshot: AnalysisSnapshot) => `报警依据是：${snapshot.reasons.join("；")}。趋势：${snapshot.trends.join("；")}`,
    mq2: (latest: SensorRecord, thresholds: ThresholdProfile) => `MQ2 当前为 ${latest.mq2Raw}，预警阈值 ${thresholds.smokeWarn}，危险阈值 ${thresholds.smokeDanger}。${latest.mq2Raw >= thresholds.smokeDanger ? "已经进入危险区间。" : latest.mq2Raw >= thresholds.smokeWarn ? "已经进入预警区间。" : "暂未触发烟雾阈值。"}`,
    rain: (latest: SensorRecord, thresholds: ThresholdProfile) => `雨量 ADC 当前为 ${latest.rainRaw ?? "--"}，湿触发阈值 ${thresholds.rainWet}。${latest.rainWet ? "雨量模块已经触发湿态。" : "雨量模块暂未触发湿态。"}`,
    therm: (latest: SensorRecord, thresholds: ThresholdProfile) => `热敏温度当前为 ${Number.isFinite(latest.thermC10) ? ((latest.thermC10 as number) / 10).toFixed(1) : "--"}°C，预警阈值 ${(thresholds.thermWarnC10 / 10).toFixed(1)}°C，危险阈值 ${(thresholds.thermDangerC10 / 10).toFixed(1)}°C。${latest.thermHot ? "DO 高温已经触发。" : "DO 高温暂未触发。"}`,
    next: (snapshot: AnalysisSnapshot) => `建议：${snapshot.recommendations.join("；")}`,
    default: (snapshot: AnalysisSnapshot) => `我看到当前状态是「${snapshot.riskLabel}」。${snapshot.summary} 建议：${snapshot.recommendations.join("；")}`,
    emptyReply: "后端没有返回可显示的回复。",
    missingDirectKey: "请先填写 DeepSeek API Key。本地演示时 key 只保存在当前浏览器标签页。",
  },
  en: {
    localProvider: "Local rules",
    deepSeekProvider: "DeepSeek",
    directProvider: "DeepSeek direct",
    proxyProvider: "DeepSeek proxy",
    hybridProvider: "DeepSeek direct / Local rules",
    proxyHybridProvider: "DeepSeek proxy / Local rules",
    fallbackProvider: "Local fallback",
    noData: "No valid data is available yet. Start replay or connect Board B serial first.",
    stale: "I would not call it safe. The data is stale, so check board communication and Board B serial output.",
    safe: (snapshot: AnalysisSnapshot) => `Current risk level is ${snapshot.riskLabel}. ${snapshot.summary} Evidence: ${snapshot.reasons.join("; ")}`,
    alarm: (snapshot: AnalysisSnapshot) => `Alarm evidence: ${snapshot.reasons.join("; ")}. Trend: ${snapshot.trends.join("; ")}`,
    mq2: (latest: SensorRecord, thresholds: ThresholdProfile) => `MQ2 is ${latest.mq2Raw}; warning threshold is ${thresholds.smokeWarn}, danger threshold is ${thresholds.smokeDanger}. ${latest.mq2Raw >= thresholds.smokeDanger ? "It is in the danger range." : latest.mq2Raw >= thresholds.smokeWarn ? "It is in the warning range." : "It has not crossed the smoke threshold."}`,
    rain: (latest: SensorRecord, thresholds: ThresholdProfile) => `Rain ADC is ${latest.rainRaw ?? "--"}; wet threshold is ${thresholds.rainWet}. ${latest.rainWet ? "The rain module is wet-triggered." : "The rain module is not wet-triggered."}`,
    therm: (latest: SensorRecord, thresholds: ThresholdProfile) => `Thermistor is ${Number.isFinite(latest.thermC10) ? ((latest.thermC10 as number) / 10).toFixed(1) : "--"}°C; warning threshold is ${(thresholds.thermWarnC10 / 10).toFixed(1)}°C and danger threshold is ${(thresholds.thermDangerC10 / 10).toFixed(1)}°C. ${latest.thermHot ? "The DO high-temperature output is triggered." : "The DO high-temperature output is not triggered."}`,
    next: (snapshot: AnalysisSnapshot) => `Recommendation: ${snapshot.recommendations.join("; ")}`,
    default: (snapshot: AnalysisSnapshot) => `Current state is ${snapshot.riskLabel}. ${snapshot.summary} Recommendation: ${snapshot.recommendations.join("; ")}`,
    emptyReply: "The backend returned no displayable reply.",
    missingDirectKey: "Enter a DeepSeek API key first. For local demos it is kept only in this browser tab.",
  },
};

function p(locale: Locale) {
  return phrases[locale] ?? phrases.en;
}

function lastUserText(messages: Array<Pick<ChatMessage, "role" | "content">>): string {
  const last = [...messages].reverse().find((message) => message.role === "user");
  return String(last?.content ?? "").toLowerCase();
}

function safeMessage(message: Pick<ChatMessage, "role" | "content">): BackendMessage {
  return {
    role: message.role === "assistant" ? "assistant" : message.role === "system" ? "system" : "user",
    content: String(message.content ?? "").slice(0, 2000),
  };
}

function compactFrame(frame: SensorRecord | null): Partial<SensorRecord> | null {
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
    selectedThresholdSensor: frame.selectedThresholdSensor,
    thresholdAirLevel: frame.thresholdAirLevel,
    thresholdSmokeLevel: frame.thresholdSmokeLevel,
    thresholdRainLevel: frame.thresholdRainLevel,
    thresholdThermLevel: frame.thresholdThermLevel,
    thresholdAirWarn: frame.thresholdAirWarn,
    thresholdSmokeWarn: frame.thresholdSmokeWarn,
    thresholdSmokeDanger: frame.thresholdSmokeDanger,
    thresholdRainWet: frame.thresholdRainWet,
    thresholdThermWarnC10: frame.thresholdThermWarnC10,
    thresholdThermDangerC10: frame.thresholdThermDangerC10,
    mute: frame.mute,
    flashReady: frame.flashReady,
    flashRecords: frame.flashRecords,
    schemaVersion: frame.schemaVersion,
    receivedAt: frame.receivedAt,
  };
}

function compactSnapshot(snapshot: AnalysisSnapshot) {
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

function systemPrompt(locale: Locale): string {
  if (locale === "zh-CN") {
    return [
      "你是双 STM32F103 环境安全监测系统的值守助手。",
      "只基于当前传感器快照和最近历史回答，不编造硬件状态。",
      "回答要短、明确，并优先说明风险等级、关键证据和下一步动作。",
      "可以使用简短 Markdown 列表、加粗和代码样式组织答案，不要输出 HTML。",
    ].join("");
  }

  return [
    "You are the duty assistant for a dual STM32F103 environmental safety monitor.",
    "Answer only from the current sensor snapshot and recent history; do not invent hardware state.",
    "Keep replies concise and prioritize risk level, evidence, and next action.",
    "You may use concise Markdown lists, bold text, and code spans; do not output HTML.",
  ].join(" ");
}

function buildBackendMessages(
  messages: Array<Pick<ChatMessage, "role" | "content">>,
  snapshot: AnalysisSnapshot,
  locale: Locale,
): BackendMessage[] {
  return [
    { role: "system", content: systemPrompt(locale) },
    {
      role: "system",
      content: `Current sensor analysis snapshot:\n${JSON.stringify(compactSnapshot(snapshot), null, 2)}`,
    },
    ...messages.slice(-10).map(safeMessage),
  ];
}

function extractAssistantContent(payload: unknown): string {
  if (!payload || typeof payload !== "object") {
    return typeof payload === "string" ? payload : "";
  }

  const data = payload as Record<string, any>;
  const direct = data.content ?? data.reply ?? data.answer ?? data.text;
  if (typeof direct === "string") {
    return direct;
  }

  const messageContent = data.message?.content ?? data.data?.message?.content ?? data.data?.content;
  if (typeof messageContent === "string") {
    return messageContent;
  }

  const choiceContent = data.choices?.[0]?.message?.content ?? data.choices?.[0]?.delta?.content;
  return typeof choiceContent === "string" ? choiceContent : "";
}

function normalizeAssistantReply(payload: unknown, provider: string, locale: Locale): AssistantReply {
  if (payload && typeof payload === "object") {
    const data = payload as Record<string, unknown>;
    if (data.role === "assistant" && typeof data.content === "string") {
      return {
        role: "assistant",
        content: data.content,
        provider: typeof data.provider === "string" ? data.provider : provider,
      };
    }
  }

  const content = extractAssistantContent(payload).trim();
  const data = payload && typeof payload === "object" ? payload as Record<string, unknown> : {};
  return {
    role: "assistant",
    content: content || p(locale).emptyReply,
    provider: typeof data.provider === "string" ? data.provider : provider,
  };
}

async function readResponsePayload(response: Response): Promise<unknown> {
  const contentType = response.headers.get("Content-Type") ?? "";
  if (contentType.toLowerCase().includes("application/json")) {
    return response.json();
  }

  return response.text();
}

function withStatusError(message: string, status: number): Error & { status: number } {
  const error = new Error(message) as Error & { status: number };
  error.status = status;
  return error;
}

async function fetchJsonWithTimeout(url: string, options: RequestInit, timeoutMs: number): Promise<Response> {
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), timeoutMs);

  try {
    return await fetch(url, { ...options, signal: controller.signal });
  } finally {
    clearTimeout(timeout);
  }
}

export class LocalInsightProvider implements AiProvider {
  id = "local-rules";

  analyze(snapshot: AnalysisSnapshot, locale: Locale = "zh-CN"): Insight {
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

  async chat({
    messages,
    snapshot,
    locale = "zh-CN",
  }: {
    messages: Array<Pick<ChatMessage, "role" | "content">>;
    snapshot: AnalysisSnapshot;
    locale?: Locale;
  }): Promise<AssistantReply> {
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
        content: t.mq2(snapshot.latest, thresholdsForRecord(snapshot.latest)),
        provider: t.localProvider,
      };
    }
    if (text.includes("雨") || text.includes("rain")) {
      return {
        role: "assistant",
        content: t.rain(snapshot.latest, thresholdsForRecord(snapshot.latest)),
        provider: t.localProvider,
      };
    }
    if (text.includes("热敏") || text.includes("therm") || text.includes("ntc")) {
      return {
        role: "assistant",
        content: t.therm(snapshot.latest, thresholdsForRecord(snapshot.latest)),
        provider: t.localProvider,
      };
    }
    if (text.includes("下一步") || text.includes("排查") || text.includes("next") || text.includes("check")) {
      return { role: "assistant", content: t.next(snapshot), provider: t.localProvider };
    }
    return { role: "assistant", content: t.default(snapshot), provider: t.localProvider };
  }
}

export class DeepSeekProvider implements AiProvider {
  id = "deepseek";
  private endpoint: string;
  private model: string;
  private enabled: boolean;
  private timeoutMs: number;
  private fallbackToLocal: boolean;
  private localProvider = new LocalInsightProvider();

  constructor({
    endpoint = "/api/ai/chat",
    model = "deepseek-v4-flash",
    enabled = true,
    timeoutMs = 12000,
    fallbackToLocal = true,
  }: {
    endpoint?: string;
    model?: string;
    enabled?: boolean;
    timeoutMs?: number;
    fallbackToLocal?: boolean;
  } = {}) {
    this.endpoint = endpoint;
    this.model = model;
    this.enabled = enabled;
    this.timeoutMs = timeoutMs;
    this.fallbackToLocal = fallbackToLocal;
  }

  analyze(snapshot: AnalysisSnapshot, locale: Locale = "zh-CN"): Insight {
    const insight = this.localProvider.analyze(snapshot, locale);
    return {
      ...insight,
      provider: this.enabled ? p(locale).proxyHybridProvider : p(locale).localProvider,
    };
  }

  async chat({
    messages,
    snapshot,
    locale = "zh-CN",
  }: {
    messages: Array<Pick<ChatMessage, "role" | "content">>;
    snapshot: AnalysisSnapshot;
    locale?: Locale;
  }): Promise<AssistantReply> {
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
        throw withStatusError(`DeepSeek proxy HTTP ${response.status}`, response.status);
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
        fallbackReason: error instanceof Error ? error.message : String(error),
      };
    }
  }
}

export class DirectDeepSeekProvider implements AiProvider {
  id = "deepseek-direct";
  private endpoint: string;
  private model: string;
  private apiKeyProvider: () => string;
  private timeoutMs: number;
  private fallbackToLocal: boolean;
  private localProvider = new LocalInsightProvider();

  constructor({
    endpoint = "https://api.deepseek.com/chat/completions",
    model = "deepseek-v4-flash",
    apiKeyProvider = () => "",
    timeoutMs = 12000,
    fallbackToLocal = true,
  }: {
    endpoint?: string;
    model?: string;
    apiKeyProvider?: () => string;
    timeoutMs?: number;
    fallbackToLocal?: boolean;
  } = {}) {
    this.endpoint = endpoint;
    this.model = model;
    this.apiKeyProvider = apiKeyProvider;
    this.timeoutMs = timeoutMs;
    this.fallbackToLocal = fallbackToLocal;
  }

  analyze(snapshot: AnalysisSnapshot, locale: Locale = "zh-CN"): Insight {
    const insight = this.localProvider.analyze(snapshot, locale);
    return {
      ...insight,
      provider: p(locale).hybridProvider,
    };
  }

  async chat({
    messages,
    snapshot,
    locale = "zh-CN",
  }: {
    messages: Array<Pick<ChatMessage, "role" | "content">>;
    snapshot: AnalysisSnapshot;
    locale?: Locale;
  }): Promise<AssistantReply> {
    const apiKey = String(this.apiKeyProvider() ?? "").trim();
    if (!apiKey) {
      throw new Error(p(locale).missingDirectKey);
    }

    const requestBody = {
      model: this.model,
      messages: buildBackendMessages(messages, snapshot, locale),
      stream: false,
      temperature: 0.2,
    };

    try {
      const response = await fetchJsonWithTimeout(this.endpoint, {
        method: "POST",
        headers: {
          Authorization: `Bearer ${apiKey}`,
          "Content-Type": "application/json",
        },
        body: JSON.stringify(requestBody),
      }, this.timeoutMs);

      if (!response.ok) {
        const payload = await readResponsePayload(response);
        const message = extractAssistantContent(payload) || (typeof payload === "string" ? payload : "");
        throw withStatusError(`DeepSeek direct HTTP ${response.status}${message ? `: ${message.slice(0, 160)}` : ""}`, response.status);
      }

      const payload = await readResponsePayload(response);
      return normalizeAssistantReply(payload, p(locale).directProvider, locale);
    } catch (error) {
      const status = typeof error === "object" && error !== null && "status" in error ? Number((error as { status: number }).status) : 0;
      if (!this.fallbackToLocal || status === 401 || status === 403) {
        throw error;
      }

      const reply = await this.localProvider.chat({ messages, snapshot, locale });
      return {
        ...reply,
        provider: p(locale).fallbackProvider,
        fallbackReason: error instanceof Error ? error.message : String(error),
      };
    }
  }
}
