export type Locale = "zh-CN" | "en";

export type AlarmState = "normal" | "warn" | "danger" | "node_lost";

export type RiskLevel = "unknown" | "normal" | "warning" | "danger" | "offline";

export type AiMode = "direct" | "proxy" | "local";

export type SourceMode = "idle" | "serial" | "replay";

export type SourceStatus = "connected" | "disconnected" | "loop";

export type EventType = "info" | "warn" | "danger" | "error" | "normal" | "node_lost";

export interface ThresholdProfile {
  airWarn: number;
  smokeWarn: number;
  smokeDanger: number;
  rainWet: number;
  thermWarnC10: number;
  thermDangerC10: number;
}

export interface SensorRecord {
  type: "sensor";
  schemaVersion: 1 | 2;
  seq: number;
  tickMs: number;
  tempC: number;
  humidityPct: number;
  mq135Raw: number;
  mq2Raw: number;
  flame: number;
  status: number;
  thresholdProfile: number;
  selectedThresholdSensor?: number;
  thresholdAirLevel?: number;
  thresholdSmokeLevel?: number;
  thresholdRainLevel?: number;
  thresholdThermLevel?: number;
  thresholdAirWarn?: number;
  thresholdSmokeWarn?: number;
  thresholdSmokeDanger?: number;
  thresholdRainWet?: number;
  thresholdThermWarnC10?: number;
  thresholdThermDangerC10?: number;
  mute: number;
  flashReady: number;
  rainRaw: number | null;
  thermRaw: number | null;
  thermC10: number | null;
  rainWet: number;
  thermHot: number;
  flashRecords: number | null;
  alarm: AlarmState;
  receivedAt: number;
}

export type ParseResult =
  | { kind: "ignored"; raw: unknown }
  | { kind: "error"; error: string; raw: unknown }
  | { kind: "sensor"; data: SensorRecord; raw: unknown };

export interface AnalysisSnapshot {
  latest: SensorRecord | null;
  history: SensorRecord[];
  stale: boolean;
  riskLevel: RiskLevel;
  riskLabel: string;
  summary: string;
  reasons: string[];
  trends: string[];
  recommendations: string[];
  thresholds: ThresholdProfile;
  generatedAt: number;
  freshness: "empty" | "live" | "stale";
}

export interface Insight {
  provider: string;
  riskLevel: RiskLevel;
  title: string;
  summary: string;
  reasons: string[];
  trends: string[];
  recommendations: string[];
}

export interface ChatMessage {
  id: string;
  role: "user" | "assistant" | "system";
  content: string;
  provider?: string;
  pending?: boolean;
  fallbackReason?: string;
}

export interface BackendMessage {
  role: "user" | "assistant" | "system";
  content: string;
}

export interface AssistantReply {
  role: "assistant";
  content: string;
  provider: string;
  fallbackReason?: string;
}

export interface AiProvider {
  id: string;
  analyze(snapshot: AnalysisSnapshot, locale?: Locale): Insight;
  chat(args: {
    messages: Array<Pick<ChatMessage, "role" | "content">>;
    snapshot: AnalysisSnapshot;
    locale?: Locale;
  }): Promise<AssistantReply>;
}

export interface AiConfig {
  mode: AiMode;
  proxyEndpoint: string;
  directEndpoint: string;
  model: string;
  timeoutMs: number;
  fallbackToLocal: boolean;
}

export interface EventRow {
  id: string;
  time: string;
  message: string;
  type: EventType;
}

export interface SerialSourceCallbacks {
  onLine?: (line: string) => void;
  onStatus?: (status: SourceStatus) => void;
  onError?: (error: unknown) => void;
}

export interface MetricDefinition {
  key: keyof SensorRecord;
  labelKey: string;
  unit: string;
  tone: string;
}

export interface ChartSeriesDefinition {
  key: keyof SensorRecord;
  labelKey: string;
  shortLabel: string;
  unit: string;
  color: string;
  yAxisIndex: 0 | 1;
  area?: boolean;
  value: (record: SensorRecord) => number | null;
}

export interface RuntimeSafetyMonitorConfig {
  ai?: Partial<AiConfig> & { endpoint?: string };
  mode?: AiMode;
  proxyEndpoint?: string;
  directEndpoint?: string;
  endpoint?: string;
  model?: string;
  timeoutMs?: number;
  fallbackToLocal?: boolean;
}

declare global {
  interface Window {
    SAFETY_MONITOR_CONFIG?: RuntimeSafetyMonitorConfig;
  }
}
