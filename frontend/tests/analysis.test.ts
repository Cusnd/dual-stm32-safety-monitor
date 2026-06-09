import { describe, expect, it } from "vitest";
import { LocalInsightProvider } from "../src/aiProvider";
import { buildAnalysisSnapshot } from "../src/analysis";
import type { SensorRecord } from "../src/types";

function frame(overrides: Partial<SensorRecord> = {}): SensorRecord {
  return {
    type: "sensor",
    seq: 1,
    tickMs: 1000,
    schemaVersion: 2,
    tempC: 26,
    humidityPct: 50,
    mq135Raw: 1000,
    mq2Raw: 800,
    rainRaw: 900,
    thermRaw: 1500,
    thermC10: 260,
    rainWet: 0,
    thermHot: 0,
    flame: 0,
    status: 0,
    alarm: "normal",
    thresholdProfile: 0,
    mute: 0,
    flashReady: 1,
    flashRecords: 0,
    receivedAt: Date.now(),
    ...overrides,
  };
}

describe("local analysis", () => {
  it("builds an unknown snapshot without data", () => {
    const snapshot = buildAnalysisSnapshot({ latest: null, history: [], locale: "en", now: 1 });
    expect(snapshot.riskLevel).toBe("unknown");
    expect(snapshot.freshness).toBe("empty");
    expect(snapshot.summary).toMatch(/No valid/);
  });

  it("detects danger from MQ2 danger threshold", () => {
    const latest = frame({ mq2Raw: 3000, alarm: "danger" });
    const snapshot = buildAnalysisSnapshot({ latest, history: [frame({ mq2Raw: 900 }), latest], locale: "en", now: 1 });

    expect(snapshot.riskLevel).toBe("danger");
    expect(snapshot.reasons.some((reason) => reason.includes("MQ2=3000"))).toBe(true);
    expect(snapshot.trends.some((trend) => trend.includes("MQ2"))).toBe(true);
  });

  it("uses live per-sensor thresholds when the firmware reports them", () => {
    const latest = frame({
      mq2Raw: 2300,
      alarm: "danger",
      thresholdProfile: 255,
      thresholdSmokeWarn: 1200,
      thresholdSmokeDanger: 2200,
      thresholdAirWarn: 2200,
      thresholdRainWet: 1400,
      thresholdThermWarnC10: 450,
      thresholdThermDangerC10: 700,
    });
    const snapshot = buildAnalysisSnapshot({ latest, history: [latest], locale: "en", now: 1 });

    expect(snapshot.riskLevel).toBe("danger");
    expect(snapshot.thresholds.smokeDanger).toBe(2200);
    expect(snapshot.reasons.some((reason) => reason.includes("threshold 2200"))).toBe(true);
  });

  it("detects warning from DHT status bit and MQ135", () => {
    const latest = frame({ mq135Raw: 2300, status: 1, alarm: "warn" });
    const snapshot = buildAnalysisSnapshot({ latest, history: [latest], locale: "en", now: 1 });

    expect(snapshot.riskLevel).toBe("warning");
    expect(snapshot.reasons.some((reason) => reason.includes("DHT11"))).toBe(true);
  });

  it("detects warning from rain wet trigger", () => {
    const latest = frame({ rainRaw: 1600, rainWet: 1, status: 4, alarm: "warn" });
    const snapshot = buildAnalysisSnapshot({ latest, history: [latest], locale: "en", now: 1 });

    expect(snapshot.riskLevel).toBe("warning");
    expect(snapshot.reasons.some((reason) => reason.includes("rain"))).toBe(true);
  });

  it("detects danger from thermistor high temperature", () => {
    const latest = frame({ thermC10: 720, thermHot: 1, status: 2, alarm: "danger" });
    const snapshot = buildAnalysisSnapshot({ latest, history: [frame({ thermC10: 430 }), latest], locale: "en", now: 1 });

    expect(snapshot.riskLevel).toBe("danger");
    expect(snapshot.reasons.some((reason) => reason.includes("thermistor"))).toBe(true);
  });

  it("stale data wins over the last live reading", () => {
    const latest = frame({ mq2Raw: 800, alarm: "normal" });
    const snapshot = buildAnalysisSnapshot({ latest, history: [latest], stale: true, locale: "en", now: 1 });

    expect(snapshot.riskLevel).toBe("offline");
    expect(snapshot.freshness).toBe("stale");
  });

  it("local provider answers user questions from the current snapshot", async () => {
    const provider = new LocalInsightProvider();
    const latest = frame({ mq2Raw: 1900, alarm: "warn" });
    const snapshot = buildAnalysisSnapshot({ latest, history: [latest], locale: "zh-CN", now: 1 });

    const answer = await provider.chat({
      messages: [{ role: "user", content: "现在安全吗？" }],
      snapshot,
      locale: "zh-CN",
    });

    expect(answer.role).toBe("assistant");
    expect(answer.content).toMatch(/风险等级/);
    expect(answer.content).toMatch(/预警/);
  });
});
