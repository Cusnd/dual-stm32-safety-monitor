import { translate } from "./i18n";
import type { AlarmState, Locale, ParseResult, SensorRecord } from "./types";

const REQUIRED_NUMBERS = [
  "seq",
  "tickMs",
  "tempC",
  "humidityPct",
  "mq135Raw",
  "mq2Raw",
  "flame",
  "status",
  "thresholdProfile",
  "mute",
  "flashReady",
] as const;

const V2_REQUIRED_NUMBERS = [
  "rainRaw",
  "thermRaw",
  "thermC10",
  "rainWet",
  "thermHot",
  "flashRecords",
] as const;

const OPTIONAL_THRESHOLD_NUMBERS = [
  "selectedThresholdSensor",
  "thresholdAirLevel",
  "thresholdSmokeLevel",
  "thresholdRainLevel",
  "thresholdThermLevel",
  "thresholdAirWarn",
  "thresholdSmokeWarn",
  "thresholdSmokeDanger",
  "thresholdRainWet",
  "thresholdThermWarnC10",
  "thresholdThermDangerC10",
] as const;

const ALARM_STATES = new Set<AlarmState>(["normal", "warn", "danger", "node_lost"]);

function toInteger(value: unknown, field: string): number {
  const number = Number(value);
  if (!Number.isFinite(number) || !Number.isInteger(number)) {
    throw new Error(`${field} must be an integer`);
  }
  return number;
}

function normalizeSensorRecord(record: Record<string, unknown>): SensorRecord {
  if (!record || typeof record !== "object") {
    throw new Error("record must be an object");
  }
  if (record.type !== "sensor") {
    throw new Error("type must be sensor");
  }

  const schemaVersion = "schemaVersion" in record ? toInteger(record.schemaVersion, "schemaVersion") : 1;
  if (schemaVersion !== 1 && schemaVersion !== 2) {
    throw new Error("schemaVersion must be 1 or 2");
  }

  const normalized: Partial<SensorRecord> = { type: "sensor", schemaVersion };
  for (const field of REQUIRED_NUMBERS) {
    if (!(field in record)) {
      throw new Error(`missing field: ${field}`);
    }
    normalized[field] = toInteger(record[field], field);
  }

  if (schemaVersion >= 2) {
    for (const field of V2_REQUIRED_NUMBERS) {
      if (!(field in record)) {
        throw new Error(`missing field: ${field}`);
      }
      normalized[field] = toInteger(record[field], field);
    }
  } else {
    normalized.rainRaw = null;
    normalized.thermRaw = null;
    normalized.thermC10 = null;
    normalized.rainWet = 0;
    normalized.thermHot = 0;
    normalized.flashRecords = null;
  }

  for (const field of OPTIONAL_THRESHOLD_NUMBERS) {
    if (field in record) {
      normalized[field] = toInteger(record[field], field);
    }
  }

  if (typeof record.alarm !== "string" || !ALARM_STATES.has(record.alarm as AlarmState)) {
    throw new Error("alarm must be normal, warn, danger, or node_lost");
  }

  normalized.alarm = record.alarm as AlarmState;
  normalized.receivedAt = Date.now();
  return normalized as SensorRecord;
}

export function parseSerialLine(line: unknown): ParseResult {
  const text = String(line ?? "").trim();
  if (text.length === 0 || !text.startsWith("{")) {
    return { kind: "ignored", raw: line };
  }

  try {
    const parsed = JSON.parse(text) as Record<string, unknown>;
    return { kind: "sensor", data: normalizeSensorRecord(parsed), raw: line };
  } catch (error) {
    return {
      kind: "error",
      error: error instanceof Error ? error.message : String(error),
      raw: line,
    };
  }
}

export function parseSerialText(text: unknown): ParseResult[] {
  return String(text ?? "")
    .split(/\r?\n/)
    .map((line) => parseSerialLine(line))
    .filter((result) => result.kind !== "ignored");
}

export function alarmLabel(alarm: AlarmState, locale = "zh-CN"): string {
  const labels = {
    "zh-CN": {
      normal: "正常",
      warn: "预警",
      danger: "危险",
      node_lost: "节点离线",
    },
    en: {
      normal: "Normal",
      warn: "Warning",
      danger: "Danger",
      node_lost: "Node lost",
    },
  };
  return labels[locale as "zh-CN" | "en"]?.[alarm] ?? labels.en[alarm] ?? alarm;
}

export function parserErrorLabel(error: string, locale: Locale = "zh-CN"): string {
  if (error === "record must be an object") {
    return translate(locale, "parserRecordObject");
  }
  if (error === "type must be sensor") {
    return translate(locale, "parserTypeSensor");
  }
  if (error === "schemaVersion must be 1 or 2") {
    return translate(locale, "parserSchemaVersion");
  }
  if (error === "alarm must be normal, warn, danger, or node_lost") {
    return translate(locale, "parserAlarm");
  }

  const missing = /^missing field: (.+)$/.exec(error);
  if (missing) {
    return `${translate(locale, "parserMissingField")}: ${missing[1]}`;
  }

  const integer = /^(.+) must be an integer$/.exec(error);
  if (integer) {
    return `${integer[1]} ${translate(locale, "parserIntegerField")}`;
  }

  return `${translate(locale, "parserInvalidJson")}: ${error}`;
}
