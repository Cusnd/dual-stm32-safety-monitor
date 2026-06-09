import { AppShell, Container, Grid, Group, Stack, Text, Title } from "@mantine/core";
import { useEffect } from "react";
import { DetailsPanel } from "./components/DetailsPanel";
import { DsAssistant } from "./components/DsAssistant";
import { EventsPanel } from "./components/EventsPanel";
import { HeaderControls } from "./components/HeaderControls";
import { MetricGrid } from "./components/MetricGrid";
import { StatusStrip } from "./components/StatusStrip";
import { TrendChart } from "./components/TrendChart";
import { useDashboard } from "./hooks/useDashboard";

export default function App() {
  const dashboard = useDashboard();

  useEffect(() => {
    document.documentElement.lang = dashboard.locale;
    document.title = dashboard.t("title");
  }, [dashboard.locale, dashboard.t]);

  return (
    <AppShell header={{ height: { base: 142, sm: 98 } }} padding="md" className="app-shell">
      <AppShell.Header className="app-header">
        <Container size="xl" h="100%">
          <Group h="100%" justify="space-between" align="center" gap="md">
            <div className="brand-block">
              <Text className="eyebrow">{dashboard.t("subtitle")}</Text>
              <Title order={1}>{dashboard.t("title")}</Title>
            </div>
            <HeaderControls dashboard={dashboard} />
          </Group>
        </Container>
      </AppShell.Header>
      <AppShell.Main>
        <Container size="xl">
          <Stack gap="md">
            <StatusStrip dashboard={dashboard} />
            <MetricGrid dashboard={dashboard} />
            <Grid gutter="md" align="stretch">
              <Grid.Col span={{ base: 12, lg: 8 }}>
                <Stack gap="md">
                  <TrendChart dashboard={dashboard} />
                  <EventsPanel dashboard={dashboard} />
                </Stack>
              </Grid.Col>
              <Grid.Col span={{ base: 12, lg: 4 }}>
                <Stack gap="md">
                  <DetailsPanel dashboard={dashboard} />
                </Stack>
              </Grid.Col>
            </Grid>
          </Stack>
        </Container>
      </AppShell.Main>
      <DsAssistant dashboard={dashboard} />
    </AppShell>
  );
}
