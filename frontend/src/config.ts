import type { AiConfig, AiMode, RuntimeSafetyMonitorConfig } from "./types";

const AI_MODE_STORAGE_KEY = "safety-monitor-ai-mode";
const DIRECT_API_KEY_SESSION_KEY = "safety-monitor-deepseek-api-key";

const DEFAULT_AI_CONFIG: AiConfig = {
  mode: "direct",
  proxyEndpoint: "/api/ai/chat",
  directEndpoint: "https://api.deepseek.com/chat/completions",
  model: "deepseek-v4-flash",
  timeoutMs: 12000,
  fallbackToLocal: true,
};

function runtimeConfig(): Partial<RuntimeSafetyMonitorConfig> {
  const globalConfig = globalThis as typeof globalThis & { SAFETY_MONITOR_CONFIG?: RuntimeSafetyMonitorConfig };
  const config = globalThis.window?.SAFETY_MONITOR_CONFIG ?? globalConfig.SAFETY_MONITOR_CONFIG;
  return config?.ai ?? config ?? {};
}

function searchParams(): URLSearchParams {
  try {
    return new URLSearchParams(globalThis.location?.search ?? "");
  } catch {
    return new URLSearchParams();
  }
}

function storedMode(): string | null {
  try {
    return globalThis.localStorage?.getItem(AI_MODE_STORAGE_KEY) ?? null;
  } catch {
    return null;
  }
}

export function normalizeAiMode(mode: unknown): AiMode {
  if (mode === "local") {
    return "local";
  }
  if (mode === "proxy") {
    return "proxy";
  }
  return "direct";
}

function numberValue(value: unknown, fallback: number): number {
  const parsed = Number(value);
  return Number.isFinite(parsed) && parsed > 0 ? parsed : fallback;
}

export function resolveAiConfig(): AiConfig {
  const params = searchParams();
  const runtime = runtimeConfig();

  return {
    mode: normalizeAiMode(params.get("ai") ?? storedMode() ?? runtime.mode ?? DEFAULT_AI_CONFIG.mode),
    proxyEndpoint:
      params.get("aiProxyEndpoint") ??
      params.get("aiEndpoint") ??
      runtime.proxyEndpoint ??
      runtime.endpoint ??
      DEFAULT_AI_CONFIG.proxyEndpoint,
    directEndpoint:
      params.get("aiDirectEndpoint") ??
      params.get("aiEndpoint") ??
      runtime.directEndpoint ??
      DEFAULT_AI_CONFIG.directEndpoint,
    model: params.get("aiModel") ?? runtime.model ?? DEFAULT_AI_CONFIG.model,
    timeoutMs: numberValue(params.get("aiTimeoutMs") ?? runtime.timeoutMs, DEFAULT_AI_CONFIG.timeoutMs),
    fallbackToLocal: runtime.fallbackToLocal ?? DEFAULT_AI_CONFIG.fallbackToLocal,
  };
}

export function persistAiMode(mode: AiMode): void {
  try {
    globalThis.localStorage?.setItem(AI_MODE_STORAGE_KEY, normalizeAiMode(mode));
  } catch {
    // Storage may be unavailable in private or file-based contexts.
  }
}

export function getDirectApiKey(): string {
  try {
    return globalThis.sessionStorage?.getItem(DIRECT_API_KEY_SESSION_KEY) ?? "";
  } catch {
    return "";
  }
}

export function hasDirectApiKey(): boolean {
  return getDirectApiKey().length > 0;
}

export function persistDirectApiKey(apiKey: unknown): void {
  const cleanKey = String(apiKey ?? "").trim();
  try {
    if (cleanKey) {
      globalThis.sessionStorage?.setItem(DIRECT_API_KEY_SESSION_KEY, cleanKey);
    }
  } catch {
    // Session storage may be unavailable; the UI will simply ask again.
  }
}

export function clearDirectApiKey(): void {
  try {
    globalThis.sessionStorage?.removeItem(DIRECT_API_KEY_SESSION_KEY);
  } catch {
    // Session storage may be unavailable.
  }
}
