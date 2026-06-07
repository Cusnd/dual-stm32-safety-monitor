import assert from "node:assert/strict";
import { test } from "node:test";
import { LocalInsightProvider } from "../src/aiProvider.js";
import { buildAnalysisSnapshot } from "../src/analysis.js";

function frame(overrides = {}) {
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
    externalRgb: 1,
    receivedAt: Date.now(),
    ...overrides,
  };
}

test("builds an unknown snapshot without data", () => {
  const snapshot = buildAnalysisSnapshot({ latest: null, history: [], locale: "en", now: 1 });
  assert.equal(snapshot.riskLevel, "unknown");
  assert.equal(snapshot.freshness, "empty");
  assert.match(snapshot.summary, /No valid/);
});

test("detects danger from MQ2 danger threshold", () => {
  const latest = frame({ mq2Raw: 3000, alarm: "danger" });
  const snapshot = buildAnalysisSnapshot({ latest, history: [frame({ mq2Raw: 900 }), latest], locale: "en", now: 1 });

  assert.equal(snapshot.riskLevel, "danger");
  assert.ok(snapshot.reasons.some((reason) => reason.includes("MQ2=3000")));
  assert.ok(snapshot.trends.some((trend) => trend.includes("MQ2")));
});

test("detects warning from DHT status bit and MQ135", () => {
  const latest = frame({ mq135Raw: 2300, status: 1, alarm: "warn" });
  const snapshot = buildAnalysisSnapshot({ latest, history: [latest], locale: "en", now: 1 });

  assert.equal(snapshot.riskLevel, "warning");
  assert.ok(snapshot.reasons.some((reason) => reason.includes("DHT11")));
});

test("detects warning from rain wet trigger", () => {
  const latest = frame({ rainRaw: 1600, rainWet: 1, status: 4, alarm: "warn" });
  const snapshot = buildAnalysisSnapshot({ latest, history: [latest], locale: "en", now: 1 });

  assert.equal(snapshot.riskLevel, "warning");
  assert.ok(snapshot.reasons.some((reason) => reason.includes("rain")));
});

test("detects danger from thermistor high temperature", () => {
  const latest = frame({ thermC10: 720, thermHot: 1, status: 2, alarm: "danger" });
  const snapshot = buildAnalysisSnapshot({ latest, history: [frame({ thermC10: 430 }), latest], locale: "en", now: 1 });

  assert.equal(snapshot.riskLevel, "danger");
  assert.ok(snapshot.reasons.some((reason) => reason.includes("thermistor")));
});

test("stale data wins over the last live reading", () => {
  const latest = frame({ mq2Raw: 800, alarm: "normal" });
  const snapshot = buildAnalysisSnapshot({ latest, history: [latest], stale: true, locale: "en", now: 1 });

  assert.equal(snapshot.riskLevel, "offline");
  assert.equal(snapshot.freshness, "stale");
});

test("local provider answers user questions from the current snapshot", async () => {
  const provider = new LocalInsightProvider();
  const latest = frame({ mq2Raw: 1900, alarm: "warn" });
  const snapshot = buildAnalysisSnapshot({ latest, history: [latest], locale: "zh-CN", now: 1 });

  const answer = await provider.chat({
    messages: [{ role: "user", content: "现在安全吗？" }],
    snapshot,
    locale: "zh-CN",
  });

  assert.equal(answer.role, "assistant");
  assert.match(answer.content, /风险等级/);
  assert.match(answer.content, /预警/);
});
