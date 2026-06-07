const THRESHOLD_PROFILES = [
  { airWarn: 2200, smokeWarn: 1800, smokeDanger: 2800, rainWet: 1400, thermWarnC10: 450, thermDangerC10: 700 },
  { airWarn: 1800, smokeWarn: 1400, smokeDanger: 2400, rainWet: 1200, thermWarnC10: 400, thermDangerC10: 650 },
  { airWarn: 2600, smokeWarn: 2200, smokeDanger: 3300, rainWet: 1800, thermWarnC10: 500, thermDangerC10: 750 },
];

const messages = {
  "zh-CN": {
    unknownLabel: "等待数据",
    normalLabel: "正常",
    warningLabel: "预警",
    dangerLabel: "危险",
    offlineLabel: "节点离线",
    unknownSummary: "还没有可分析的传感器帧。",
    normalSummary: "当前传感器读数处于监测阈值以内。",
    warningSummary: "系统发现预警信号，需要继续观察并检查现场。",
    dangerSummary: "系统检测到高风险信号，需要立即处理。",
    offlineSummary: "数据已经超时，不能把最后一帧当作实时状态。",
    noDataReason: "没有收到有效 JSON Lines 数据。",
    staleReason: "超过 3 秒没有收到新的有效传感器帧。",
    flameReason: "火焰传感器处于触发状态。",
    dhtReason: "DHT11 状态位提示读取异常。",
    rainReason: (value, limit) => `雨量 ADC=${value} 已达到湿触发阈值 ${limit}。`,
    rainSwitchReason: "雨量状态位提示已经触发湿态。",
    thermDanger: (value, limit) => `热敏温度 ${value}°C 已达到危险阈值 ${limit}°C。`,
    thermWarn: (value, limit) => `热敏温度 ${value}°C 已超过预警阈值 ${limit}°C。`,
    thermSwitchReason: "热敏 DO 状态位提示高温触发。",
    thermAdcReason: "热敏 ADC 状态位提示模拟量异常。",
    mq2Danger: (value, limit) => `MQ2=${value} 已达到危险阈值 ${limit}。`,
    mq2Warn: (value, limit) => `MQ2=${value} 已超过预警阈值 ${limit}。`,
    mq135Warn: (value, limit) => `MQ135=${value} 已超过空气质量预警阈值 ${limit}。`,
    normalReason: "MQ135、MQ2、火焰、雨量和热敏状态没有触发当前阈值。",
    trendStable: "最近窗口内主要指标变化平稳。",
    trendRising: (name, delta) => `${name} 最近窗口上升 ${delta}。`,
    trendFalling: (name, delta) => `${name} 最近窗口下降 ${Math.abs(delta)}。`,
    recWait: "启动模拟串口或连接板 B CH340C 串口后再分析。",
    recOffline: "检查板 A 供电、USART3 交叉线、GND 共地和板 B 接收状态。",
    recDanger: "先远离风险源，关闭蜂鸣静音后排查烟雾、火焰或高温触发原因。",
    recWarning: "保持观察，检查 MQ 预热、雨量探头、热敏接线和 DHT11 状态。",
    recNormal: "保持监控；若要演示报警，可用安全方式改变 MQ、火焰、雨量或热敏输入。",
  },
  en: {
    unknownLabel: "Waiting",
    normalLabel: "Normal",
    warningLabel: "Warning",
    dangerLabel: "Danger",
    offlineLabel: "Node lost",
    unknownSummary: "No valid sensor frame is available yet.",
    normalSummary: "Current readings are inside the monitor thresholds.",
    warningSummary: "A warning signal is present and should be watched.",
    dangerSummary: "A high-risk signal is present and needs immediate action.",
    offlineSummary: "Data is stale, so the last frame must not be treated as live.",
    noDataReason: "No valid JSON Lines data has been received.",
    staleReason: "No fresh valid sensor frame arrived for more than 3 seconds.",
    flameReason: "The flame sensor is triggered.",
    dhtReason: "The DHT11 status bit reports a read error.",
    rainReason: (value, limit) => `Rain ADC=${value} reached wet threshold ${limit}.`,
    rainSwitchReason: "The rain status bit reports wet trigger.",
    thermDanger: (value, limit) => `Thermistor ${value}°C reached danger threshold ${limit}°C.`,
    thermWarn: (value, limit) => `Thermistor ${value}°C exceeded warning threshold ${limit}°C.`,
    thermSwitchReason: "The thermistor DO status bit reports high temperature.",
    thermAdcReason: "The thermistor ADC status bit reports an analog fault.",
    mq2Danger: (value, limit) => `MQ2=${value} reached danger threshold ${limit}.`,
    mq2Warn: (value, limit) => `MQ2=${value} exceeded warning threshold ${limit}.`,
    mq135Warn: (value, limit) => `MQ135=${value} exceeded air warning threshold ${limit}.`,
    normalReason: "MQ135, MQ2, flame, rain, and thermistor state do not trigger the current thresholds.",
    trendStable: "Main readings are stable in the recent window.",
    trendRising: (name, delta) => `${name} rose by ${delta} in the recent window.`,
    trendFalling: (name, delta) => `${name} fell by ${Math.abs(delta)} in the recent window.`,
    recWait: "Start replay or connect the Board B CH340C serial port before analysis.",
    recOffline: "Check Board A power, USART3 cross-wiring, common GND, and Board B reception.",
    recDanger: "Move away from the source first, then inspect smoke, flame, or high-temperature causes after muting is handled.",
    recWarning: "Keep watching; check MQ warm-up, the rain probe, thermistor wiring, and DHT11 status.",
    recNormal: "Keep monitoring; use safe sensor stimulation if you need to demonstrate alarms.",
  },
};

function msg(locale) {
  return messages[locale] ?? messages.en;
}

export function thresholdForProfile(profile) {
  return THRESHOLD_PROFILES[profile] ?? THRESHOLD_PROFILES[0];
}

function recentDelta(history, key) {
  const values = history.slice(-5).filter((item) => Number.isFinite(item[key]));
  if (values.length < 2) {
    return 0;
  }
  return values[values.length - 1][key] - values[0][key];
}

function addTrend(trends, locale, name, delta, minimum) {
  const t = msg(locale);
  if (Math.abs(delta) < minimum) {
    return;
  }
  trends.push(delta > 0 ? t.trendRising(name, delta) : t.trendFalling(name, delta));
}

function c10ToC(value) {
  return Number.isFinite(value) ? (value / 10).toFixed(1) : "--";
}

export function buildAnalysisSnapshot({
  latest,
  history = [],
  stale = false,
  locale = "zh-CN",
  now = Date.now(),
}) {
  const t = msg(locale);
  const cleanHistory = Array.isArray(history) ? history : [];

  if (!latest) {
    return {
      latest: null,
      history: cleanHistory,
      stale,
      riskLevel: "unknown",
      riskLabel: t.unknownLabel,
      summary: t.unknownSummary,
      reasons: [t.noDataReason],
      trends: [t.trendStable],
      recommendations: [t.recWait],
      thresholds: thresholdForProfile(0),
      generatedAt: now,
      freshness: "empty",
    };
  }

  const thresholds = thresholdForProfile(latest.thresholdProfile);
  const reasons = [];
  const trends = [];
  const recommendations = [];
  let riskLevel = "normal";

  if (stale || latest.alarm === "node_lost") {
    riskLevel = "offline";
    reasons.push(t.staleReason);
    recommendations.push(t.recOffline);
  } else {
    if (latest.flame !== 0) {
      riskLevel = "danger";
      reasons.push(t.flameReason);
    }

    if (latest.mq2Raw >= thresholds.smokeDanger) {
      riskLevel = "danger";
      reasons.push(t.mq2Danger(latest.mq2Raw, thresholds.smokeDanger));
    } else if (latest.mq2Raw >= thresholds.smokeWarn) {
      if (riskLevel !== "danger") {
        riskLevel = "warning";
      }
      reasons.push(t.mq2Warn(latest.mq2Raw, thresholds.smokeWarn));
    }

    if (latest.mq135Raw >= thresholds.airWarn) {
      if (riskLevel === "normal") {
        riskLevel = "warning";
      }
      reasons.push(t.mq135Warn(latest.mq135Raw, thresholds.airWarn));
    }

    if ((latest.rainWet !== 0) || ((latest.status & 0x04) !== 0)) {
      if (riskLevel === "normal") {
        riskLevel = "warning";
      }
      reasons.push(t.rainSwitchReason);
    } else if (Number.isFinite(latest.rainRaw) && latest.rainRaw >= thresholds.rainWet) {
      if (riskLevel === "normal") {
        riskLevel = "warning";
      }
      reasons.push(t.rainReason(latest.rainRaw, thresholds.rainWet));
    }

    if ((latest.thermHot !== 0) || ((latest.status & 0x02) !== 0)) {
      riskLevel = "danger";
      reasons.push(t.thermSwitchReason);
    } else if (Number.isFinite(latest.thermC10) && latest.thermC10 >= thresholds.thermDangerC10) {
      riskLevel = "danger";
      reasons.push(t.thermDanger(c10ToC(latest.thermC10), c10ToC(thresholds.thermDangerC10)));
    } else if (Number.isFinite(latest.thermC10) && latest.thermC10 >= thresholds.thermWarnC10) {
      if (riskLevel !== "danger") {
        riskLevel = "warning";
      }
      reasons.push(t.thermWarn(c10ToC(latest.thermC10), c10ToC(thresholds.thermWarnC10)));
    }

    if ((latest.status & 0x08) !== 0) {
      if (riskLevel === "normal") {
        riskLevel = "warning";
      }
      reasons.push(t.thermAdcReason);
    }

    if ((latest.status & 0x01) !== 0) {
      if (riskLevel === "normal") {
        riskLevel = "warning";
      }
      reasons.push(t.dhtReason);
    }
  }

  if (reasons.length === 0) {
    reasons.push(t.normalReason);
  }

  addTrend(trends, locale, "MQ2", recentDelta(cleanHistory, "mq2Raw"), 100);
  addTrend(trends, locale, "MQ135", recentDelta(cleanHistory, "mq135Raw"), 100);
  addTrend(trends, locale, "Temp", recentDelta(cleanHistory, "tempC"), 1);
  addTrend(trends, locale, "Humidity", recentDelta(cleanHistory, "humidityPct"), 2);
  addTrend(trends, locale, "Rain", recentDelta(cleanHistory, "rainRaw"), 100);
  addTrend(trends, locale, "Therm", recentDelta(cleanHistory, "thermC10"), 10);
  if (trends.length === 0) {
    trends.push(t.trendStable);
  }

  if (recommendations.length === 0) {
    if (riskLevel === "danger") {
      recommendations.push(t.recDanger);
    } else if (riskLevel === "warning") {
      recommendations.push(t.recWarning);
    } else {
      recommendations.push(t.recNormal);
    }
  }

  const labels = {
    normal: t.normalLabel,
    warning: t.warningLabel,
    danger: t.dangerLabel,
    offline: t.offlineLabel,
  };
  const summaries = {
    normal: t.normalSummary,
    warning: t.warningSummary,
    danger: t.dangerSummary,
    offline: t.offlineSummary,
  };

  return {
    latest,
    history: cleanHistory,
    stale,
    riskLevel,
    riskLabel: labels[riskLevel],
    summary: summaries[riskLevel],
    reasons,
    trends,
    recommendations,
    thresholds,
    generatedAt: now,
    freshness: stale ? "stale" : "live",
  };
}
