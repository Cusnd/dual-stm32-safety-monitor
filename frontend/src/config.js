const AI_MODE_STORAGE_KEY = "safety-monitor-ai-mode";

const DEFAULT_AI_CONFIG = {
  mode: "deepseek",
  endpoint: "/api/ai/chat",
  model: "deepseek-v4-flash",
  timeoutMs: 12000,
  fallbackToLocal: true,
};

function runtimeConfig() {
  return globalThis.SAFETY_MONITOR_CONFIG?.ai ?? globalThis.SAFETY_MONITOR_CONFIG ?? {};
}

function searchParams() {
  try {
    return new URLSearchParams(globalThis.location?.search ?? "");
  } catch {
    return new URLSearchParams();
  }
}

function storedMode() {
  try {
    return globalThis.localStorage?.getItem(AI_MODE_STORAGE_KEY);
  } catch {
    return null;
  }
}

function normalizeMode(mode) {
  return mode === "local" ? "local" : "deepseek";
}

function numberValue(value, fallback) {
  const parsed = Number(value);
  return Number.isFinite(parsed) && parsed > 0 ? parsed : fallback;
}

export function resolveAiConfig() {
  const params = searchParams();
  const runtime = runtimeConfig();

  return {
    mode: normalizeMode(params.get("ai") ?? storedMode() ?? runtime.mode ?? DEFAULT_AI_CONFIG.mode),
    endpoint: params.get("aiEndpoint") ?? runtime.endpoint ?? DEFAULT_AI_CONFIG.endpoint,
    model: params.get("aiModel") ?? runtime.model ?? DEFAULT_AI_CONFIG.model,
    timeoutMs: numberValue(params.get("aiTimeoutMs") ?? runtime.timeoutMs, DEFAULT_AI_CONFIG.timeoutMs),
    fallbackToLocal: runtime.fallbackToLocal ?? DEFAULT_AI_CONFIG.fallbackToLocal,
  };
}

export function persistAiMode(mode) {
  try {
    globalThis.localStorage?.setItem(AI_MODE_STORAGE_KEY, normalizeMode(mode));
  } catch {
    // Storage may be unavailable in private or file-based contexts.
  }
}
