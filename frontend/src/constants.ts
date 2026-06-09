import type { ChartSeriesDefinition, MetricDefinition, SensorRecord } from "./types";

export const MAX_HISTORY = 80;
export const MAX_EVENTS = 12;
export const MAX_CHAT_MESSAGES = 12;
export const STALE_AFTER_MS = 3000;

export const metricDefinitions: MetricDefinition[] = [
  { key: "tempC", labelKey: "temp", unit: "C", tone: "temp" },
  { key: "humidityPct", labelKey: "humidity", unit: "%", tone: "humidity" },
  { key: "mq135Raw", labelKey: "mq135", unit: "raw", tone: "air" },
  { key: "mq2Raw", labelKey: "mq2", unit: "raw", tone: "smoke" },
  { key: "rainRaw", labelKey: "rain", unit: "raw", tone: "rain" },
  { key: "thermC10", labelKey: "therm", unit: "C", tone: "therm" },
  { key: "flame", labelKey: "flame", unit: "", tone: "flame" },
  { key: "alarm", labelKey: "alarm", unit: "", tone: "alarm" },
];

function numericValue(record: SensorRecord, key: keyof SensorRecord): number | null {
  const value = record[key];
  return typeof value === "number" && Number.isFinite(value) ? value : null;
}

export const chartSeriesDefinitions: ChartSeriesDefinition[] = [
  {
    key: "tempC",
    labelKey: "temp",
    shortLabel: "TEMP",
    unit: "C",
    color: "#0f766e",
    yAxisIndex: 0,
    value: (record) => numericValue(record, "tempC"),
  },
  {
    key: "humidityPct",
    labelKey: "humidity",
    shortLabel: "HUM",
    unit: "%",
    color: "#2563eb",
    yAxisIndex: 0,
    value: (record) => numericValue(record, "humidityPct"),
  },
  {
    key: "thermC10",
    labelKey: "therm",
    shortLabel: "NTC",
    unit: "C",
    color: "#be123c",
    yAxisIndex: 0,
    value: (record) => {
      const value = numericValue(record, "thermC10");
      return value === null ? null : value / 10;
    },
  },
  {
    key: "mq2Raw",
    labelKey: "mq2",
    shortLabel: "MQ2",
    unit: "raw",
    color: "#dc6803",
    yAxisIndex: 1,
    area: true,
    value: (record) => numericValue(record, "mq2Raw"),
  },
  {
    key: "mq135Raw",
    labelKey: "mq135",
    shortLabel: "MQ135",
    unit: "raw",
    color: "#7c3aed",
    yAxisIndex: 1,
    area: true,
    value: (record) => numericValue(record, "mq135Raw"),
  },
  {
    key: "rainRaw",
    labelKey: "rain",
    shortLabel: "RAIN",
    unit: "raw",
    color: "#0891b2",
    yAxisIndex: 1,
    value: (record) => numericValue(record, "rainRaw"),
  },
];

