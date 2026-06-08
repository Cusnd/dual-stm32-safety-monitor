import assert from "node:assert/strict";
import { afterEach, test } from "node:test";
import { DeepSeekProvider } from "../src/aiProvider.js";
import { buildAnalysisSnapshot } from "../src/analysis.js";

const originalFetch = globalThis.fetch;

function frame(overrides = {}) {
  return {
    type: "sensor",
    seq: 7,
    tickMs: 1200,
    schemaVersion: 2,
    tempC: 31,
    humidityPct: 58,
    mq135Raw: 1200,
    mq2Raw: 900,
    rainRaw: 880,
    thermRaw: 1800,
    thermC10: 310,
    rainWet: 0,
    thermHot: 0,
    flame: 0,
    status: 0,
    alarm: "normal",
    thresholdProfile: 0,
    mute: 0,
    flashReady: 1,
    flashRecords: 4,
    externalRgb: 1,
    receivedAt: Date.now(),
    ...overrides,
  };
}

function snapshot(overrides = {}) {
  const latest = frame(overrides);
  return buildAnalysisSnapshot({ latest, history: [latest], locale: "zh-CN", now: 1 });
}

afterEach(() => {
  globalThis.fetch = originalFetch;
});

test("deepseek provider posts sensor context to backend proxy", async () => {
  let requestUrl = "";
  let requestBody = null;

  globalThis.fetch = async (url, options) => {
    requestUrl = String(url);
    requestBody = JSON.parse(options.body);
    return new Response(JSON.stringify({
      choices: [{ message: { role: "assistant", content: "当前正常，继续监控。" } }],
    }), {
      status: 200,
      headers: { "Content-Type": "application/json" },
    });
  };

  const provider = new DeepSeekProvider({
    endpoint: "/api/ai/chat",
    model: "deepseek-v4-flash",
    fallbackToLocal: false,
  });
  const reply = await provider.chat({
    messages: [{ role: "user", content: "现在安全吗？" }],
    snapshot: snapshot(),
    locale: "zh-CN",
  });

  assert.equal(requestUrl, "/api/ai/chat");
  assert.equal(requestBody.model, "deepseek-v4-flash");
  assert.equal(requestBody.apiKey, undefined);
  assert.equal(requestBody.requestType, "chat");
  assert.ok(requestBody.messages.some((message) => message.role === "system" && message.content.includes("sensor")));
  assert.equal(requestBody.snapshot.latest.seq, 7);
  assert.equal(reply.role, "assistant");
  assert.equal(reply.content, "当前正常，继续监控。");
  assert.equal(reply.provider, "DeepSeek");
});

test("deepseek provider falls back to local rules when backend fails", async () => {
  globalThis.fetch = async () => new Response("{}", { status: 503 });

  const provider = new DeepSeekProvider({ endpoint: "/api/ai/chat", fallbackToLocal: true });
  const reply = await provider.chat({
    messages: [{ role: "user", content: "现在安全吗？" }],
    snapshot: snapshot({ mq2Raw: 1900, alarm: "warn" }),
    locale: "zh-CN",
  });

  assert.equal(reply.role, "assistant");
  assert.equal(reply.provider, "本地规则兜底");
  assert.match(reply.content, /预警/);
  assert.match(reply.fallbackReason, /HTTP 503/);
});

test("deepseek provider accepts plain text backend replies", async () => {
  globalThis.fetch = async () => new Response("保持监控。", { status: 200 });

  const provider = new DeepSeekProvider({ endpoint: "/api/ai/chat", fallbackToLocal: false });
  const reply = await provider.chat({
    messages: [{ role: "user", content: "下一步？" }],
    snapshot: snapshot(),
    locale: "zh-CN",
  });

  assert.equal(reply.role, "assistant");
  assert.equal(reply.provider, "DeepSeek");
  assert.equal(reply.content, "保持监控。");
});
