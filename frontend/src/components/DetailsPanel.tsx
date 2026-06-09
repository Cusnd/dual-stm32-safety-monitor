import { Card, SimpleGrid, Text, Title } from "@mantine/core";
import type { DashboardState } from "../hooks/useDashboard";
import type { SensorRecord } from "../types";

const THRESHOLD_SENSOR_LABELS = ["MQ135", "MQ2", "RAIN", "THERM"];

function levelLabel(value: number | undefined): string {
  return Number.isFinite(value) ? `${(value as number) + 1}/5` : "--";
}

function selectedThresholdSensor(record: SensorRecord | null): string {
  if (!record || !Number.isFinite(record.selectedThresholdSensor)) {
    return "--";
  }

  return THRESHOLD_SENSOR_LABELS[record.selectedThresholdSensor as number] ?? "--";
}

function thresholdValues(record: SensorRecord | null): string {
  if (
    !record ||
    !Number.isFinite(record.thresholdAirWarn) ||
    !Number.isFinite(record.thresholdSmokeWarn) ||
    !Number.isFinite(record.thresholdSmokeDanger) ||
    !Number.isFinite(record.thresholdRainWet) ||
    !Number.isFinite(record.thresholdThermWarnC10) ||
    !Number.isFinite(record.thresholdThermDangerC10)
  ) {
    return "--";
  }

  return `AIR ${record.thresholdAirWarn} MQ2 ${record.thresholdSmokeWarn}/${record.thresholdSmokeDanger} R ${record.thresholdRainWet} T ${((record.thresholdThermWarnC10 as number) / 10).toFixed(1)}/${((record.thresholdThermDangerC10 as number) / 10).toFixed(1)}C`;
}

export function DetailsPanel({ dashboard }: { dashboard: DashboardState }) {
  const latest = dashboard.latest;
  const rows: Array<[string, string | number]> = [
    [dashboard.t("profile"), latest ? latest.thresholdProfile : "--"],
    [dashboard.t("thresholdSelected"), selectedThresholdSensor(latest)],
    [
      dashboard.t("thresholdLevels"),
      latest
        ? `A${levelLabel(latest.thresholdAirLevel)} M${levelLabel(latest.thresholdSmokeLevel)} R${levelLabel(latest.thresholdRainLevel)} T${levelLabel(latest.thresholdThermLevel)}`
        : "--",
    ],
    [
      dashboard.t("thresholdValues"),
      thresholdValues(latest),
    ],
    [dashboard.t("mute"), latest ? (latest.mute ? dashboard.t("yes") : dashboard.t("no")) : "--"],
    [dashboard.t("flash"), latest ? (latest.flashReady ? dashboard.t("ready") : "--") : "--"],
    [dashboard.t("flashRecords"), latest && latest.flashRecords !== null ? latest.flashRecords : "--"],
    [dashboard.t("rainWet"), latest ? (latest.rainWet ? dashboard.t("yes") : dashboard.t("no")) : "--"],
    [dashboard.t("thermHot"), latest ? (latest.thermHot ? dashboard.t("yes") : dashboard.t("no")) : "--"],
    [dashboard.t("schema"), latest ? `v${latest.schemaVersion}` : "--"],
    [dashboard.t("seq"), latest ? latest.seq : "--"],
    [dashboard.t("statusField"), latest ? `0x${latest.status.toString(16).padStart(2, "0").toUpperCase()}` : "--"],
    [dashboard.t("tick"), latest ? `${latest.tickMs} ms` : "--"],
  ];

  return (
    <Card className="panel-card details-panel" withBorder>
      <Title order={2}>{dashboard.t("details")}</Title>
      <SimpleGrid cols={2} spacing="xs" mt="sm">
        {rows.map(([label, value]) => (
          <div className="detail-item" key={label}>
            <Text size="xs" c="dimmed">{label}</Text>
            <Text fw={800}>{value}</Text>
          </div>
        ))}
      </SimpleGrid>
    </Card>
  );
}
