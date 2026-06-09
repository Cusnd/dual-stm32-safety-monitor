import { Badge, Button, Card, Group, PasswordInput, SegmentedControl, Stack, Text, Title } from "@mantine/core";
import { notifications } from "@mantine/notifications";
import { KeyRound, RotateCcw, Save } from "lucide-react";
import { useState } from "react";
import type { ReactNode } from "react";
import type { DashboardState } from "../hooks/useDashboard";

function BulletList({ items }: { items: string[] }) {
  return (
    <ul className="compact-list">
      {items.map((item) => (
        <li key={item}>{item}</li>
      ))}
    </ul>
  );
}

export function AiPanel({ dashboard, framed = true }: { dashboard: DashboardState; framed?: boolean }) {
  const [apiKey, setApiKey] = useState("");
  const directMode = dashboard.aiConfig.mode === "direct";

  const saveKey = () => {
    if (dashboard.saveApiKey(apiKey)) {
      setApiKey("");
      notifications.show({ color: "teal", message: dashboard.t("apiKeySaved") });
    }
  };

  const clearKey = () => {
    dashboard.clearApiKey();
    setApiKey("");
    notifications.show({ color: "gray", message: dashboard.t("apiKeyCleared") });
  };

  const content: ReactNode = (
    <>
      <Group justify="space-between" align="flex-start" mb="sm">
        <div>
          <Title order={2}>{dashboard.t("aiInsights")}</Title>
          <Text size="xs" c="dimmed">{dashboard.t("provider")}</Text>
        </div>
        <Badge color="teal" variant="light">{dashboard.insight.provider}</Badge>
      </Group>

      <SegmentedControl
        fullWidth
        value={dashboard.aiConfig.mode}
        onChange={(value) => dashboard.setAiMode(value as typeof dashboard.aiConfig.mode)}
        data={[
          { label: dashboard.t("aiModeDirect"), value: "direct" },
          { label: dashboard.t("aiModeProxy"), value: "proxy" },
          { label: dashboard.t("aiModeLocal"), value: "local" },
        ]}
      />

      {directMode && (
        <Group mt="sm" gap="xs" align="flex-end" wrap="wrap" className="api-key-row">
          <PasswordInput
            className="api-key-input"
            leftSection={<KeyRound size={16} />}
            label={dashboard.t("directApiKey")}
            placeholder={dashboard.hasDirectApiKey ? dashboard.t("apiKeySavedPlaceholder") : dashboard.t("apiKeyPlaceholder")}
            value={apiKey}
            onChange={(event) => setApiKey(event.currentTarget.value)}
            onKeyDown={(event) => {
              if (event.key === "Enter") {
                event.preventDefault();
                saveKey();
              }
            }}
          />
          <Button leftSection={<Save size={16} />} onClick={saveKey}>{dashboard.t("saveKey")}</Button>
          <Button leftSection={<RotateCcw size={16} />} color="gray" variant="light" onClick={clearKey}>{dashboard.t("clearKey")}</Button>
        </Group>
      )}

      <div className={`risk-strip risk-${dashboard.snapshot.riskLevel}`}>
        <Text size="xs" c="dimmed">{dashboard.t("aiRisk")}</Text>
        <strong>{dashboard.insight.title}</strong>
      </div>
      <Text className="ai-summary">{dashboard.insight.summary}</Text>

      <Stack gap="xs" mt="sm">
        <section>
          <Text fw={800}>{dashboard.t("aiEvidence")}</Text>
          <BulletList items={dashboard.insight.reasons} />
        </section>
        <section>
          <Text fw={800}>{dashboard.t("aiTrends")}</Text>
          <BulletList items={dashboard.insight.trends} />
        </section>
        <section>
          <Text fw={800}>{dashboard.t("aiActions")}</Text>
          <BulletList items={dashboard.insight.recommendations} />
        </section>
      </Stack>
    </>
  );

  if (!framed) {
    return <section className="assistant-section ai-panel">{content}</section>;
  }

  return (
    <Card className="panel-card ai-panel" withBorder>
      {content}
    </Card>
  );
}
