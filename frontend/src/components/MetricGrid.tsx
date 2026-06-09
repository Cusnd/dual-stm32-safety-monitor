import { Badge, Card, Group, SimpleGrid, Text } from "@mantine/core";
import { metricDefinitions } from "../constants";
import { formatMetricValue } from "../format";
import { translate, type TranslationKey } from "../i18n";
import type { DashboardState } from "../hooks/useDashboard";

export function MetricGrid({ dashboard }: { dashboard: DashboardState }) {
  return (
    <SimpleGrid cols={{ base: 2, sm: 4 }} spacing="sm" className="metric-grid" aria-label={dashboard.t("metricGridLabel")}>
      {metricDefinitions.map((metric) => {
        const value = formatMetricValue(dashboard.locale, dashboard.latest, metric.key, dashboard.stale);
        return (
          <Card key={String(metric.key)} className={`metric-card metric-${metric.tone}`} withBorder>
            <Group justify="space-between" align="flex-start" gap="xs" wrap="nowrap">
              <Text size="sm" c="dimmed" fw={700}>
                {translate(dashboard.locale, metric.labelKey as TranslationKey)}
              </Text>
              {metric.unit && dashboard.latest && (
                <Badge size="xs" variant="light" color="gray">
                  {metric.unit}
                </Badge>
              )}
            </Group>
            <Text className="metric-value">{value}</Text>
          </Card>
        );
      })}
    </SimpleGrid>
  );
}
