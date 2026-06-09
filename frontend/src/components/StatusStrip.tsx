import { Badge, Group, Paper, SimpleGrid, Text } from "@mantine/core";
import { Activity, Clock3, Radio } from "lucide-react";
import { formatClock } from "../i18n";
import type { DashboardState } from "../hooks/useDashboard";

function dataLabel(dashboard: DashboardState): string {
  if (!dashboard.latest) {
    return dashboard.t("noData");
  }
  return dashboard.stale ? dashboard.t("stale") : dashboard.t("live");
}

function connectionLabel(dashboard: DashboardState): string {
  if (dashboard.sourceMode === "serial") {
    return dashboard.t("connected");
  }
  if (dashboard.sourceMode === "replay") {
    return dashboard.t("replaying");
  }
  return dashboard.t("disconnected");
}

export function StatusStrip({ dashboard }: { dashboard: DashboardState }) {
  const last = dashboard.latest ? formatClock(dashboard.locale, dashboard.latest.receivedAt) : "--";

  return (
    <SimpleGrid cols={{ base: 1, sm: 3 }} spacing="sm" className="status-strip">
      <Paper className="status-tile" withBorder>
        <Group gap="sm" wrap="nowrap">
          <Radio size={20} />
          <div>
            <Text size="xs" c="dimmed">{dashboard.t("serial")}</Text>
            <Badge color={dashboard.sourceMode === "idle" ? "gray" : "teal"} variant="light">
              {connectionLabel(dashboard)}
            </Badge>
          </div>
        </Group>
      </Paper>
      <Paper className="status-tile" withBorder>
        <Group gap="sm" wrap="nowrap">
          <Activity size={20} />
          <div>
            <Text size="xs" c="dimmed">{dashboard.t("data")}</Text>
            <Badge color={dashboard.stale ? "yellow" : dashboard.latest ? "green" : "gray"} variant="light">
              {dataLabel(dashboard)}
            </Badge>
          </div>
        </Group>
      </Paper>
      <Paper className="status-tile" withBorder>
        <Group gap="sm" wrap="nowrap">
          <Clock3 size={20} />
          <div>
            <Text size="xs" c="dimmed">{dashboard.t("lastUpdate")}</Text>
            <Text fw={700}>{last}</Text>
          </div>
        </Group>
      </Paper>
    </SimpleGrid>
  );
}

