import { afterEach, describe, expect, it, vi } from "vitest";
import { DeepSeekProvider, DirectDeepSeekProvider } from "../src/aiProvider";
import { buildAnalysisSnapshot } from "../src/analysis";
import type { SensorRecord } from "../src/types";

const originalFetch = globalThis.fetch;

function frame(overrides: Partial<SensorRecord> = {}): SensorRecord {
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
    receivedAt: Date.now(),
    ...overrides,
  };
}

function snapshot(overrides: Partial<SensorRecord> = {}) {
  const latest = frame(overrides);
  return buildAnalysisSnapshot({ latest, history: [latest], locale: "zh-CN", now: 1 });
}

afterEach(() => {
  globalThis.fetch = originalFetch;
});

describe("AI providers", () => {
  it("deepseek provider posts sensor context to backend proxy", async () => {
    let requestUrl = "";
    let requestBody: any = null;

    globalThis.fetch = vi.fn(async (url: string | URL | Request, options?: RequestInit) => {
      requestUrl = String(url);
      requestBody = JSON.parse(String(options?.body));
      return new Response(JSON.stringify({
        choices: [{ message: { role: "assistant", content: "当前正常，继续监控。" } }],
      }), {
        status: 200,
        headers: { "Content-Type": "application/json" },
      });
    }) as typeof fetch;

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

    expect(requestUrl).toBe("/api/ai/chat");
    expect(requestBody.model).toBe("deepseek-v4-flash");
    expect(requestBody.apiKey).toBeUndefined();
    expect(requestBody.requestType).toBe("chat");
    expect(requestBody.messages.some((message: any) => message.role === "system" && message.content.includes("sensor"))).toBe(true);
    expect(requestBody.snapshot.latest.seq).toBe(7);
    expect(reply.role).toBe("assistant");
    expect(reply.content).toBe("当前正常，继续监控。");
    expect(reply.provider).toBe("DeepSeek");
  });

  it("deepseek provider falls back to local rules when backend fails", async () => {
    globalThis.fetch = vi.fn(async () => new Response("{}", { status: 503 })) as typeof fetch;

    const provider = new DeepSeekProvider({ endpoint: "/api/ai/chat", fallbackToLocal: true });
    const reply = await provider.chat({
      messages: [{ role: "user", content: "现在安全吗？" }],
      snapshot: snapshot({ mq2Raw: 1900, alarm: "warn" }),
      locale: "zh-CN",
    });

    expect(reply.role).toBe("assistant");
    expect(reply.provider).toBe("本地规则兜底");
    expect(reply.content).toMatch(/预警/);
    expect(reply.fallbackReason).toMatch(/HTTP 503/);
  });

  it("deepseek provider accepts plain text backend replies", async () => {
    globalThis.fetch = vi.fn(async () => new Response("保持监控。", { status: 200 })) as typeof fetch;

    const provider = new DeepSeekProvider({ endpoint: "/api/ai/chat", fallbackToLocal: false });
    const reply = await provider.chat({
      messages: [{ role: "user", content: "下一步？" }],
      snapshot: snapshot(),
      locale: "zh-CN",
    });

    expect(reply.role).toBe("assistant");
    expect(reply.provider).toBe("DeepSeek");
    expect(reply.content).toBe("保持监控。");
  });

  it("direct provider sends api key only in authorization header", async () => {
    let requestBody: any = null;
    let authorization = "";

    globalThis.fetch = vi.fn(async (_url: string | URL | Request, options?: RequestInit) => {
      requestBody = JSON.parse(String(options?.body));
      authorization = String((options?.headers as Record<string, string>).Authorization);
      return new Response(JSON.stringify({
        choices: [{ message: { role: "assistant", content: "直连正常。" } }],
      }), {
        status: 200,
        headers: { "Content-Type": "application/json" },
      });
    }) as typeof fetch;

    const provider = new DirectDeepSeekProvider({
      endpoint: "https://api.deepseek.com/chat/completions",
      apiKeyProvider: () => "sk-test-only",
      fallbackToLocal: false,
    });
    const reply = await provider.chat({
      messages: [{ role: "user", content: "现在安全吗？" }],
      snapshot: snapshot(),
      locale: "zh-CN",
    });

    expect(authorization).toBe("Bearer sk-test-only");
    expect(requestBody.apiKey).toBeUndefined();
    expect(requestBody.messages.at(-1).content).toBe("现在安全吗？");
    expect(reply.content).toBe("直连正常。");
  });

  it("direct provider requires an entered api key", async () => {
    const provider = new DirectDeepSeekProvider({ apiKeyProvider: () => "" });

    await expect(provider.chat({
      messages: [{ role: "user", content: "现在安全吗？" }],
      snapshot: snapshot(),
      locale: "zh-CN",
    })).rejects.toThrow(/请先填写 DeepSeek API Key/);
  });
});

