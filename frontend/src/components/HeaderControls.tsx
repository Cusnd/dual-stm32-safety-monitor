import { Badge, Button, Group, Tooltip } from "@mantine/core";
import { Cable, Languages, Pause, Play, Unplug } from "lucide-react";
import type { DashboardState } from "../hooks/useDashboard";

export function HeaderControls({ dashboard }: { dashboard: DashboardState }) {
  const {
    locale,
    setLocale,
    sourceMode,
    t,
    connectSerial,
    disconnectSerial,
    toggleReplay,
    support,
  } = dashboard;

  return (
    <Group gap="xs" justify="flex-end" wrap="wrap">
      {!support && (
        <Badge color="yellow" variant="light">
          {t("unsupported")}
        </Badge>
      )}
      <Button
        variant="light"
        color="gray"
        leftSection={<Languages size={17} />}
        aria-label={t("languageToggle")}
        onClick={() => setLocale(locale === "zh-CN" ? "en" : "zh-CN")}
      >
        {locale === "zh-CN" ? "EN" : "中文"}
      </Button>
      <Tooltip label={t("serialTooltip")}>
        <Button
          color="teal"
          leftSection={<Cable size={17} />}
          onClick={() => void connectSerial()}
          disabled={sourceMode === "serial"}
        >
          {t("connect")}
        </Button>
      </Tooltip>
      <Button
        color="gray"
        variant="light"
        leftSection={<Unplug size={17} />}
        onClick={() => void disconnectSerial()}
        disabled={sourceMode !== "serial"}
      >
        {t("disconnect")}
      </Button>
      <Button
        color={sourceMode === "replay" ? "orange" : "blue"}
        variant={sourceMode === "replay" ? "filled" : "light"}
        leftSection={sourceMode === "replay" ? <Pause size={17} /> : <Play size={17} />}
        onClick={() => void toggleReplay()}
      >
        {sourceMode === "replay" ? t("stopSimulation") : t("startSimulation")}
      </Button>
    </Group>
  );
}
