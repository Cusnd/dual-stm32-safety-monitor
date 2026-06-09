import { useCallback, useMemo, useRef, useState, useEffect } from "react";
import { DeepSeekProvider, DirectDeepSeekProvider, LocalInsightProvider } from "../aiProvider";
import { buildAnalysisSnapshot } from "../analysis";
import { MAX_CHAT_MESSAGES, MAX_EVENTS, MAX_HISTORY, STALE_AFTER_MS } from "../constants";
import {
  clearDirectApiKey,
  getDirectApiKey,
  hasDirectApiKey,
  persistAiMode,
  persistDirectApiKey,
  resolveAiConfig,
} from "../config";
import { eventTone } from "../format";
import { alarmLabel, parseSerialLine, parserErrorLabel } from "../parser";
import { ReplaySerialSource } from "../replaySerial";
import { WebSerialSource } from "../serial";
import { formatClock, translate } from "../i18n";
import type {
  AiConfig,
  AiMode,
  AiProvider,
  ChatMessage,
  EventRow,
  EventType,
  Locale,
  SensorRecord,
  SourceMode,
} from "../types";

function makeId(prefix: string): string {
  const randomId = globalThis.crypto?.randomUUID?.() ?? `${Date.now()}-${Math.random().toString(16).slice(2)}`;
  return `${prefix}-${randomId}`;
}

function createAiProvider(config: AiConfig): AiProvider {
  if (config.mode === "local") {
    return new LocalInsightProvider();
  }
  if (config.mode === "direct") {
    return new DirectDeepSeekProvider({
      endpoint: config.directEndpoint,
      model: config.model,
      apiKeyProvider: getDirectApiKey,
      timeoutMs: config.timeoutMs,
      fallbackToLocal: config.fallbackToLocal,
    });
  }

  return new DeepSeekProvider({
    endpoint: config.proxyEndpoint,
    model: config.model,
    timeoutMs: config.timeoutMs,
    fallbackToLocal: config.fallbackToLocal,
  });
}

export function useDashboard() {
  const [locale, setLocale] = useState<Locale>("zh-CN");
  const [sourceMode, setSourceMode] = useState<SourceMode>("idle");
  const [latest, setLatest] = useState<SensorRecord | null>(null);
  const [stale, setStale] = useState(false);
  const [history, setHistory] = useState<SensorRecord[]>([]);
  const [events, setEvents] = useState<EventRow[]>([]);
  const [chatMessages, setChatMessages] = useState<ChatMessage[]>([]);
  const [chatBusy, setChatBusy] = useState(false);
  const [selectedSeriesKey, setSelectedSeriesKey] = useState<string>("");
  const [aiConfig, setAiConfig] = useState<AiConfig>(() => resolveAiConfig());
  const [apiKeyVersion, setApiKeyVersion] = useState(0);

  const serialRef = useRef<WebSerialSource | null>(null);
  const replayRef = useRef<ReplaySerialSource | null>(null);

  const t = useCallback((key: Parameters<typeof translate>[1]) => translate(locale, key), [locale]);

  const formatSourceError = useCallback((error: unknown) => {
    const message = error instanceof Error ? error.message : String(error);
    if (message === "Web Serial is not supported by this browser") {
      return t("unsupported");
    }
    if (message === "Replay source has no serial lines") {
      return t("replayEmpty");
    }
    const replayHttp = /^Replay fixture failed: HTTP (.+)$/.exec(message);
    if (replayHttp) {
      return `${t("replayHttpError")}: HTTP ${replayHttp[1]}`;
    }
    return message;
  }, [t]);

  const addEvent = useCallback((message: string, type: EventType = "info") => {
    setEvents((current) => [
      {
        id: makeId("event"),
        time: formatClock(locale),
        message,
        type,
      },
      ...current,
    ].slice(0, MAX_EVENTS));
  }, [locale]);

  const ingestLine = useCallback((line: string) => {
    const result = parseSerialLine(line);
    if (result.kind === "ignored") {
      return;
    }
    if (result.kind === "error") {
      addEvent(parserErrorLabel(result.error, locale), "error");
      return;
    }

    const record = result.data;
    setLatest(record);
    setStale(false);
    setHistory((current) => [...current, record].slice(-MAX_HISTORY));
    addEvent(`seq=${record.seq} ${alarmLabel(record.alarm, locale)}`, record.alarm);
  }, [addEvent, locale]);

  const stopReplay = useCallback(async () => {
    await replayRef.current?.disconnect();
    replayRef.current = null;
    setSourceMode((current) => current === "replay" ? "idle" : current);
  }, []);

  const disconnectSerial = useCallback(async () => {
    await serialRef.current?.disconnect();
    serialRef.current = null;
    setSourceMode((current) => current === "serial" ? "idle" : current);
  }, []);

  const connectSerial = useCallback(async () => {
    try {
      await stopReplay();
      const source = new WebSerialSource({
        onLine: ingestLine,
        onStatus: (state) => {
          if (state === "connected") {
            setSourceMode("serial");
            addEvent(t("connected"));
          } else {
            setSourceMode((current) => current === "serial" ? "idle" : current);
            addEvent(t("disconnected"));
          }
        },
        onError: (error) => addEvent(formatSourceError(error), "error"),
      });
      serialRef.current = source;
      await source.connect();
    } catch (error) {
      addEvent(formatSourceError(error), "error");
    }
  }, [addEvent, formatSourceError, ingestLine, stopReplay, t]);

  const startReplay = useCallback(async () => {
    await disconnectSerial();
    const source = new ReplaySerialSource({
      onLine: ingestLine,
      onStatus: (state) => {
        if (state === "connected") {
          setSourceMode("replay");
          addEvent(t("replayStarted"));
        } else if (state === "loop") {
          addEvent(t("replayLoop"));
        } else {
          setSourceMode((current) => current === "replay" ? "idle" : current);
          addEvent(t("replayStopped"));
        }
      },
      onError: (error) => addEvent(formatSourceError(error), "error"),
      intervalMs: 700,
      loop: true,
    });
    replayRef.current = source;
    await source.connect();
  }, [addEvent, disconnectSerial, formatSourceError, ingestLine, t]);

  const toggleReplay = useCallback(async () => {
    if (sourceMode === "replay") {
      await stopReplay();
    } else {
      await startReplay();
    }
  }, [sourceMode, startReplay, stopReplay]);

  const setAiMode = useCallback((mode: AiMode) => {
    setAiConfig((current) => ({ ...current, mode }));
    persistAiMode(mode);
    addEvent(`${t("aiModeChanged")}: ${translate(locale, mode === "direct" ? "aiModeDirect" : mode === "proxy" ? "aiModeProxy" : "aiModeLocal")}`);
  }, [addEvent, locale, t]);

  const saveApiKey = useCallback((apiKey: string) => {
    const cleanKey = apiKey.trim();
    if (!cleanKey) {
      return false;
    }
    persistDirectApiKey(cleanKey);
    setApiKeyVersion((version) => version + 1);
    addEvent(t("apiKeySaved"));
    return true;
  }, [addEvent, t]);

  const clearApiKey = useCallback(() => {
    clearDirectApiKey();
    setApiKeyVersion((version) => version + 1);
    addEvent(t("apiKeyCleared"));
  }, [addEvent, t]);

  const aiProvider = useMemo(() => createAiProvider(aiConfig), [aiConfig, apiKeyVersion]);

  const snapshot = useMemo(() => buildAnalysisSnapshot({ latest, history, stale, locale }), [history, latest, locale, stale]);
  const insight = useMemo(() => aiProvider.analyze(snapshot, locale), [aiProvider, locale, snapshot]);

  const sendChat = useCallback(async (content: string) => {
    const cleanContent = content.trim();
    if (!cleanContent || chatBusy) {
      return;
    }

    const userMessage: ChatMessage = {
      id: makeId("user"),
      role: "user",
      content: cleanContent,
    };
    const pendingMessage: ChatMessage = {
      id: makeId("assistant"),
      role: "assistant",
      content: t("sending"),
      provider: insight.provider,
      pending: true,
    };
    const baseMessages = [...chatMessages, userMessage];
    setChatMessages([...baseMessages, pendingMessage].slice(-MAX_CHAT_MESSAGES));
    setChatBusy(true);

    try {
      const reply = await aiProvider.chat({
        messages: baseMessages,
        snapshot,
        locale,
      });
      if (reply.fallbackReason) {
        addEvent(t("aiFallback"), "warn");
      }
      setChatMessages([...baseMessages, { ...reply, id: makeId("assistant") }].slice(-MAX_CHAT_MESSAGES));
    } catch (error) {
      const errorMessage: ChatMessage = {
        id: makeId("assistant"),
        role: "assistant",
        content: error instanceof Error ? error.message : String(error),
        provider: insight.provider,
      };
      setChatMessages([
        ...baseMessages,
        errorMessage,
      ].slice(-MAX_CHAT_MESSAGES));
    } finally {
      setChatBusy(false);
    }
  }, [addEvent, aiProvider, chatBusy, chatMessages, insight.provider, locale, snapshot, t]);

  useEffect(() => {
    if (!latest || stale) {
      return undefined;
    }

    const remainingMs = Math.max(0, STALE_AFTER_MS - (Date.now() - latest.receivedAt) + 50);
    const timeoutId = window.setTimeout(() => {
      if (Date.now() - latest.receivedAt >= STALE_AFTER_MS) {
        setStale(true);
        addEvent(t("streamStale"), "warn");
      }
    }, remainingMs);

    return () => window.clearTimeout(timeoutId);
  }, [addEvent, latest, stale, t]);

  useEffect(() => {
    return () => {
      void serialRef.current?.disconnect();
      void replayRef.current?.disconnect();
    };
  }, []);

  const support = WebSerialSource.isSupported();
  const dataState = !latest ? "empty" : stale ? "stale" : "live";
  const connectionTone = sourceMode === "idle" ? "default" : "live";
  const riskTone = eventTone(snapshot.riskLevel === "offline" ? "node_lost" : snapshot.riskLevel === "warning" ? "warn" : snapshot.riskLevel === "danger" ? "danger" : "normal");

  return {
    locale,
    setLocale,
    t,
    sourceMode,
    connectionTone,
    latest,
    stale,
    dataState,
    history,
    events,
    support,
    chatMessages,
    chatBusy,
    selectedSeriesKey,
    setSelectedSeriesKey,
    aiConfig,
    setAiMode,
    hasDirectApiKey: hasDirectApiKey(),
    saveApiKey,
    clearApiKey,
    snapshot,
    insight,
    riskTone,
    connectSerial,
    disconnectSerial,
    toggleReplay,
    sendChat,
  };
}

export type DashboardState = ReturnType<typeof useDashboard>;
