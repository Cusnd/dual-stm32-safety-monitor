import { Badge, Card, Group, SimpleGrid, Text, Title } from "@mantine/core";
import ReactECharts from "echarts-for-react";
import type { EChartsOption } from "echarts";
import { useMemo } from "react";
import { chartSeriesDefinitions } from "../constants";
import { eventTone, formatSeriesValue, seriesByKey, seriesTitle } from "../format";
import { formatClock } from "../i18n";
import { alarmLabel } from "../parser";
import type { ChartSeriesDefinition, SensorRecord } from "../types";
import type { DashboardState } from "../hooks/useDashboard";

function maxHistoryValue(history: SensorRecord[], key: keyof SensorRecord): number | null {
  const values = history.map((item) => item[key]).filter((value): value is number => Number.isFinite(value));
  return values.length ? Math.max(...values) : null;
}

function chartName(dashboard: DashboardState, series: ChartSeriesDefinition): string {
  return seriesTitle(dashboard.locale, series);
}

function colorForAlarm(alarm: string | undefined): string {
  if (alarm === "danger") {
    return "rgba(217, 45, 32, 0.08)";
  }
  if (alarm === "warn") {
    return "rgba(220, 104, 3, 0.08)";
  }
  if (alarm === "node_lost") {
    return "rgba(37, 99, 235, 0.08)";
  }
  return "transparent";
}

function markAreas(history: SensorRecord[], stale: boolean): Array<[{ xAxis: number; itemStyle: { color: string } }, { xAxis: number }]> {
  if (history.length < 2) {
    return [];
  }

  const areas: Array<[{ xAxis: number; itemStyle: { color: string } }, { xAxis: number }]> = [];
  for (const [index, record] of history.slice(1).entries()) {
    const alarm = stale && index === history.length - 2 ? "node_lost" : record.alarm;
    const color = colorForAlarm(alarm);
    if (color !== "transparent") {
      areas.push([{ xAxis: index, itemStyle: { color } }, { xAxis: index + 1 }]);
    }
  }
  return areas;
}

export function TrendChart({ dashboard }: { dashboard: DashboardState }) {
  const selectedSeries = seriesByKey(dashboard.selectedSeriesKey);
  const selectedRows = selectedSeries
    ? dashboard.history
      .map((record) => ({ record, value: selectedSeries.value(record) }))
      .filter((row): row is { record: SensorRecord; value: number } => Number.isFinite(row.value))
      .slice(-12)
      .reverse()
    : [];

  const stats = [
    { label: dashboard.t("samples"), value: dashboard.history.length || "--", tone: "info" },
    {
      label: dashboard.t("currentAlarm"),
      value: dashboard.latest ? alarmLabel(dashboard.stale ? "node_lost" : dashboard.latest.alarm, dashboard.locale) : "--",
      tone: eventTone(dashboard.stale ? "node_lost" : dashboard.latest?.alarm),
    },
    { label: dashboard.t("peakMq2"), value: maxHistoryValue(dashboard.history, "mq2Raw") ?? "--", tone: "warn" },
    {
      label: dashboard.t("peakTherm"),
      value: Number.isFinite(maxHistoryValue(dashboard.history, "thermC10"))
        ? ((maxHistoryValue(dashboard.history, "thermC10") as number) / 10).toFixed(1)
        : "--",
      tone: "danger",
    },
  ];

  const nameToKey = useMemo(() => {
    const pairs = chartSeriesDefinitions.map((series) => [chartName(dashboard, series), String(series.key)] as const);
    return new Map(pairs);
  }, [dashboard.locale]);

  const option = useMemo<EChartsOption>(() => {
    const names = chartSeriesDefinitions.map((series) => chartName(dashboard, series));
    const xData = dashboard.history.map((record) => formatClock(dashboard.locale, record.receivedAt));
    const hasSelection = Boolean(dashboard.selectedSeriesKey);

    return {
      backgroundColor: "transparent",
      animationDuration: 260,
      color: chartSeriesDefinitions.map((series) => series.color),
      grid: {
        left: 44,
        right: 50,
        top: 48,
        bottom: 58,
        containLabel: true,
      },
      legend: {
        top: 4,
        left: 0,
        type: "scroll",
        itemWidth: 14,
        itemHeight: 8,
        textStyle: {
          color: "#344054",
          fontSize: 12,
          fontWeight: 700,
        },
        data: names,
      },
      tooltip: {
        trigger: "axis",
        confine: true,
        backgroundColor: "rgba(255, 255, 255, 0.96)",
        borderColor: "#d0d5dd",
        textStyle: { color: "#101828" },
        formatter: (params: unknown) => {
          const items = Array.isArray(params) ? params as any[] : [params as any];
          const title = items[0]?.axisValueLabel ?? items[0]?.axisValue ?? "";
          const lines = items.map((item) => {
            const value = item.value;
            return `${item.marker ?? ""}${item.seriesName}: <strong>${Number.isFinite(value) ? value : "--"}</strong>`;
          });
          return [`<strong>${title}</strong>`, ...lines].join("<br/>");
        },
      },
      xAxis: {
        type: "category",
        boundaryGap: false,
        data: xData,
        axisLine: { lineStyle: { color: "#98a2b3" } },
        axisLabel: { color: "#667085", hideOverlap: true },
      },
      yAxis: [
        {
          type: "value",
          name: "C / %",
          min: 0,
          max: 100,
          splitLine: { lineStyle: { color: "#eaecf0", type: "dashed" } },
          axisLabel: { color: "#667085" },
          nameTextStyle: { color: "#667085", fontWeight: 700 },
        },
        {
          type: "value",
          name: "ADC",
          min: 0,
          max: 4095,
          splitLine: { show: false },
          axisLabel: { color: "#667085" },
          nameTextStyle: { color: "#667085", fontWeight: 700 },
        },
      ],
      dataZoom: [
        { type: "inside", throttle: 80 },
        {
          type: "slider",
          height: 18,
          bottom: 16,
          borderColor: "#d0d5dd",
          fillerColor: "rgba(15, 118, 110, 0.14)",
          handleSize: 14,
          start: dashboard.history.length > 40 ? 45 : 0,
          end: 100,
        },
      ],
      series: chartSeriesDefinitions.map((series, index) => {
        const selected = dashboard.selectedSeriesKey === series.key;
        const muted = hasSelection && !selected;
        return {
          id: String(series.key),
          name: chartName(dashboard, series),
          type: "line",
          yAxisIndex: series.yAxisIndex,
          smooth: true,
          showSymbol: selected,
          symbolSize: selected ? 7 : 4,
          sampling: "lttb",
          connectNulls: false,
          data: dashboard.history.map((record) => series.value(record)),
          lineStyle: {
            width: selected ? 4 : 2.4,
            opacity: muted ? 0.16 : 1,
          },
          itemStyle: {
            opacity: muted ? 0.2 : 1,
          },
          areaStyle: series.area
            ? {
              opacity: muted ? 0.02 : 0.12,
            }
            : undefined,
          emphasis: {
            focus: "series",
            lineStyle: { width: 4 },
          },
          markArea: index === 0
            ? {
              silent: true,
              data: markAreas(dashboard.history, dashboard.stale),
            }
            : undefined,
        };
      }),
    };
  }, [dashboard.history, dashboard.locale, dashboard.selectedSeriesKey, dashboard.stale]);

  const onEvents = useMemo(() => ({
    click: (params: any) => {
      const key = String(params?.seriesId ?? "");
      if (key) {
        dashboard.setSelectedSeriesKey(dashboard.selectedSeriesKey === key ? "" : key);
      }
    },
    legendselectchanged: (params: any) => {
      const key = nameToKey.get(String(params?.name ?? ""));
      if (key) {
        dashboard.setSelectedSeriesKey(dashboard.selectedSeriesKey === key ? "" : key);
      }
    },
  }), [dashboard, nameToKey]);

  return (
    <Card className="panel-card trend-panel" withBorder>
      <Group justify="space-between" mb="sm" align="flex-start">
        <div>
          <Title order={2}>{dashboard.t("trend")}</Title>
          <Text size="xs" c="dimmed">{dashboard.t("selectedSensor")}: {selectedSeries ? seriesTitle(dashboard.locale, selectedSeries) : dashboard.t("noSelectedSensor")}</Text>
        </div>
        <Badge color={dashboard.history.length > 1 ? "teal" : "gray"} variant="light">
          {dashboard.t("allSensors")}
        </Badge>
      </Group>

      <SimpleGrid cols={{ base: 2, md: 4 }} spacing="xs" className="chart-stats">
        {stats.map((stat) => (
          <div key={stat.label} className={`chart-stat tone-${stat.tone}`}>
            <span>{stat.label}</span>
            <strong>{stat.value}</strong>
          </div>
        ))}
      </SimpleGrid>

      <div className="chart-shell" role="img" aria-label={dashboard.t("trend")}>
        <ReactECharts
          option={option}
          style={{ height: 370, width: "100%" }}
          notMerge
          lazyUpdate
          opts={{ renderer: "canvas" }}
          onEvents={onEvents}
        />
      </div>

      <div className="chart-history">
        <Group justify="space-between" align="center" mb="xs">
          <Text fw={800}>
            {dashboard.t("historyTitle")}
            {selectedSeries ? `: ${seriesTitle(dashboard.locale, selectedSeries)}` : ""}
          </Text>
          <Badge variant="light" color={selectedSeries ? "teal" : "gray"}>
            {selectedRows[0] && selectedSeries ? `${dashboard.t("latestValue")}: ${formatSeriesValue(selectedSeries, selectedRows[0].value)}` : dashboard.t("noSelectedSensor")}
          </Badge>
        </Group>
        {selectedRows.length === 0 ? (
          <div className="history-empty">{dashboard.t("allSensors")}</div>
        ) : (
          <ol>
            {selectedRows.map(({ record, value }) => (
              <li key={`${record.seq}-${record.receivedAt}`}>
                <time>{formatClock(dashboard.locale, record.receivedAt)}</time>
                <span>seq={record.seq}</span>
                <strong>{formatSeriesValue(selectedSeries, value)}</strong>
              </li>
            ))}
          </ol>
        )}
      </div>
    </Card>
  );
}
