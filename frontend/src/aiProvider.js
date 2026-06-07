import { thresholdForProfile } from "./analysis.js";

const phrases = {
  "zh-CN": {
    localProvider: "本地规则",
    noData: "现在还没有有效数据。先启动模拟串口或连接板 B 串口，我再帮你分析。",
    stale: "不建议判断为安全。当前数据已经超时，先检查板间通信和板 B 串口输出。",
    safe: (snapshot) => `当前风险等级是「${snapshot.riskLabel}」。${snapshot.summary} 关键依据：${snapshot.reasons.join("；")}`,
    alarm: (snapshot) => `报警依据是：${snapshot.reasons.join("；")}。趋势：${snapshot.trends.join("；")}`,
    mq2: (latest, thresholds) => `MQ2 当前为 ${latest.mq2Raw}，预警阈值 ${thresholds.smokeWarn}，危险阈值 ${thresholds.smokeDanger}。${latest.mq2Raw >= thresholds.smokeDanger ? "已经进入危险区间。" : latest.mq2Raw >= thresholds.smokeWarn ? "已经进入预警区间。" : "暂未触发烟雾阈值。"}`,
    rain: (latest, thresholds) => `雨量 ADC 当前为 ${latest.rainRaw ?? "--"}，湿触发阈值 ${thresholds.rainWet}。${latest.rainWet ? "雨量模块已经触发湿态。" : "雨量模块暂未触发湿态。"}`,
    therm: (latest, thresholds) => `热敏温度当前为 ${Number.isFinite(latest.thermC10) ? (latest.thermC10 / 10).toFixed(1) : "--"}°C，预警阈值 ${(thresholds.thermWarnC10 / 10).toFixed(1)}°C，危险阈值 ${(thresholds.thermDangerC10 / 10).toFixed(1)}°C。${latest.thermHot ? "DO 高温已经触发。" : "DO 高温暂未触发。"}`,
    next: (snapshot) => `建议：${snapshot.recommendations.join("；")}`,
    default: (snapshot) => `我看到当前状态是「${snapshot.riskLabel}」。${snapshot.summary} 建议：${snapshot.recommendations.join("；")}`,
    disabled: "DeepSeek provider is disabled; use a backend proxy before enabling it.",
  },
  en: {
    localProvider: "Local rules",
    noData: "No valid data is available yet. Start replay or connect Board B serial first.",
    stale: "I would not call it safe. The data is stale, so check board communication and Board B serial output.",
    safe: (snapshot) => `Current risk level is ${snapshot.riskLabel}. ${snapshot.summary} Evidence: ${snapshot.reasons.join("; ")}`,
    alarm: (snapshot) => `Alarm evidence: ${snapshot.reasons.join("; ")}. Trend: ${snapshot.trends.join("; ")}`,
    mq2: (latest, thresholds) => `MQ2 is ${latest.mq2Raw}; warning threshold is ${thresholds.smokeWarn}, danger threshold is ${thresholds.smokeDanger}. ${latest.mq2Raw >= thresholds.smokeDanger ? "It is in the danger range." : latest.mq2Raw >= thresholds.smokeWarn ? "It is in the warning range." : "It has not crossed the smoke threshold."}`,
    rain: (latest, thresholds) => `Rain ADC is ${latest.rainRaw ?? "--"}; wet threshold is ${thresholds.rainWet}. ${latest.rainWet ? "The rain module is wet-triggered." : "The rain module is not wet-triggered."}`,
    therm: (latest, thresholds) => `Thermistor is ${Number.isFinite(latest.thermC10) ? (latest.thermC10 / 10).toFixed(1) : "--"}°C; warning threshold is ${(thresholds.thermWarnC10 / 10).toFixed(1)}°C and danger threshold is ${(thresholds.thermDangerC10 / 10).toFixed(1)}°C. ${latest.thermHot ? "The DO high-temperature output is triggered." : "The DO high-temperature output is not triggered."}`,
    next: (snapshot) => `Recommendation: ${snapshot.recommendations.join("; ")}`,
    default: (snapshot) => `Current state is ${snapshot.riskLabel}. ${snapshot.summary} Recommendation: ${snapshot.recommendations.join("; ")}`,
    disabled: "DeepSeek provider is disabled; use a backend proxy before enabling it.",
  },
};

function p(locale) {
  return phrases[locale] ?? phrases.en;
}

function lastUserText(messages) {
  const last = [...messages].reverse().find((message) => message.role === "user");
  return String(last?.content ?? "").toLowerCase();
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
  constructor({ endpoint = "/api/ai/chat", model = "deepseek-v4-flash", enabled = false } = {}) {
    this.id = "deepseek";
    this.endpoint = endpoint;
    this.model = model;
    this.enabled = enabled;
  }

  analyze(snapshot, locale = "zh-CN") {
    return new LocalInsightProvider().analyze(snapshot, locale);
  }

  async chat({ messages, snapshot, locale = "zh-CN" }) {
    if (!this.enabled) {
      throw new Error(p(locale).disabled);
    }

    const response = await fetch(this.endpoint, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ model: this.model, messages, snapshot, locale }),
    });

    if (!response.ok) {
      throw new Error(`DeepSeek proxy failed: HTTP ${response.status}`);
    }

    return response.json();
  }
}
