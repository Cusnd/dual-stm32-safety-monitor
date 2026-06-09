import { Badge, Card, Group, ScrollArea, Text, Title } from "@mantine/core";
import { AlertTriangle, CheckCircle2, CircleAlert, Info, WifiOff } from "lucide-react";
import { eventLabel, eventTone } from "../format";
import type { DashboardState } from "../hooks/useDashboard";

function iconForTone(tone: ReturnType<typeof eventTone>) {
  if (tone === "danger") {
    return <CircleAlert size={17} />;
  }
  if (tone === "warn" || tone === "error") {
    return <AlertTriangle size={17} />;
  }
  if (tone === "offline") {
    return <WifiOff size={17} />;
  }
  if (tone === "normal") {
    return <CheckCircle2 size={17} />;
  }
  return <Info size={17} />;
}

export function EventsPanel({ dashboard }: { dashboard: DashboardState }) {
  return (
    <Card className="panel-card events-panel" withBorder>
      <Group justify="space-between" mb="sm">
        <Title order={2}>{dashboard.t("log")}</Title>
        <Badge color="gray" variant="light">{dashboard.events.length}</Badge>
      </Group>
      <ScrollArea h={dashboard.events.length > 0 ? 250 : 92} type="hover">
        {dashboard.events.length === 0 ? (
          <div className="empty-state">{dashboard.t("noEvents")}</div>
        ) : (
          <ol className="event-list">
            {dashboard.events.map((event) => {
              const tone = eventTone(event.type);
              return (
                <li key={event.id} className={`event-row tone-${tone}`}>
                  <span className="event-icon">{iconForTone(tone)}</span>
                  <time>{event.time}</time>
                  <Badge size="xs" color={tone === "danger" ? "red" : tone === "warn" || tone === "error" ? "orange" : tone === "offline" ? "blue" : tone === "normal" ? "green" : "gray"} variant="light">
                    {eventLabel(dashboard.locale, event.type)}
                  </Badge>
                  <Text component="span">{event.message}</Text>
                </li>
              );
            })}
          </ol>
        )}
      </ScrollArea>
    </Card>
  );
}

