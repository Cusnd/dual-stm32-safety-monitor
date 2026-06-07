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
];

const V2_REQUIRED_NUMBERS = [
  "rainRaw",
  "thermRaw",
  "thermC10",
  "rainWet",
  "thermHot",
  "flashRecords",
  "externalRgb",
];

const ALARM_STATES = new Set(["normal", "warn", "danger", "node_lost"]);

function toInteger(value, field) {
  const number = Number(value);
  if (!Number.isFinite(number) || !Number.isInteger(number)) {
    throw new Error(`${field} must be an integer`);
  }
  return number;
}

function normalizeSensorRecord(record) {
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

  const normalized = { type: "sensor", schemaVersion };
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
    normalized.externalRgb = 0;
  }

  if (typeof record.alarm !== "string" || !ALARM_STATES.has(record.alarm)) {
    throw new Error("alarm must be normal, warn, danger, or node_lost");
  }

  normalized.alarm = record.alarm;
  normalized.receivedAt = Date.now();
  return normalized;
}

export function parseSerialLine(line) {
  const text = String(line ?? "").trim();
  if (text.length === 0 || !text.startsWith("{")) {
    return { kind: "ignored", raw: line };
  }

  try {
    const parsed = JSON.parse(text);
    return { kind: "sensor", data: normalizeSensorRecord(parsed), raw: line };
  } catch (error) {
    return {
      kind: "error",
      error: error instanceof Error ? error.message : String(error),
      raw: line,
    };
  }
}

export function parseSerialText(text) {
  return String(text ?? "")
    .split(/\r?\n/)
    .map((line) => parseSerialLine(line))
    .filter((result) => result.kind !== "ignored");
}

export function alarmLabel(alarm, locale = "zh-CN") {
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
  return labels[locale]?.[alarm] ?? labels.en[alarm] ?? alarm;
}
