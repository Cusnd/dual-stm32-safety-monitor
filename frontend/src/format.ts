import { chartSeriesDefinitions } from "./constants";
import { alarmLabel } from "./parser";
import { translate, type TranslationKey } from "./i18n";
import type { AlarmState, ChartSeriesDefinition, EventType, Locale, SensorRecord } from "./types";

export function eventTone(type: EventType | AlarmState | undefined): "info" | "warn" | "danger" | "error" | "normal" | "offline" {
  if (type === "error") {
    return "error";
  }
  if (type === "danger") {
    return "danger";
  }
  if (type === "warn") {
    return "warn";
  }
  if (type === "normal") {
    return "normal";
  }
  if (type === "node_lost") {
    return "offline";
  }
  return "info";
}

export function eventLabel(locale: Locale, type: EventType | AlarmState | undefined): string {
  const tone = eventTone(type);
  if (tone === "error") {
    return translate(locale, "eventError");
  }
  if (tone === "danger") {
    return translate(locale, "eventDanger");
  }
  if (tone === "warn") {
    return translate(locale, "eventWarn");
  }
  if (tone === "normal") {
    return translate(locale, "eventNormal");
  }
  if (tone === "offline") {
    return translate(locale, "eventOffline");
  }
  return translate(locale, "eventInfo");
}

export function seriesTitle(locale: Locale, series: ChartSeriesDefinition): string {
  const title = translate(locale, series.labelKey as TranslationKey);
  return series.shortLabel.toLowerCase() === title.toLowerCase() ? title : `${series.shortLabel} ${title}`;
}

export function seriesByKey(key: string): ChartSeriesDefinition | null {
  return chartSeriesDefinitions.find((series) => series.key === key) ?? null;
}

export function formatSeriesValue(series: ChartSeriesDefinition | null, value: number | null): string {
  if (!series || typeof value !== "number" || !Number.isFinite(value)) {
    return "--";
  }
  const display = Number.isInteger(value) ? String(value) : value.toFixed(1);
  return `${display}${series.unit ? ` ${series.unit}` : ""}`;
}

export function formatMetricValue(locale: Locale, latest: SensorRecord | null, key: keyof SensorRecord, stale = false): string {
  if (!latest) {
    return "--";
  }
  const value = latest[key];
  if (value === null || value === undefined) {
    return "--";
  }
  if (key === "thermC10") {
    return Number.isFinite(value) ? ((value as number) / 10).toFixed(1) : "--";
  }
  if (key === "flame") {
    return latest.flame ? translate(locale, "yes") : translate(locale, "no");
  }
  if (key === "alarm") {
    return alarmLabel(stale ? "node_lost" : latest.alarm, locale);
  }
  return String(value);
}
