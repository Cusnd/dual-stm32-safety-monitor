import { ActionIcon, Group, Text, Title } from "@mantine/core";
import { Maximize2, MessageCircle, X } from "lucide-react";
import { useCallback, useEffect, useRef, useState } from "react";
import { AiPanel } from "./AiPanel";
import { ChatPanel } from "./ChatPanel";
import type { DashboardState } from "../hooks/useDashboard";
import type { PointerEvent as ReactPointerEvent } from "react";

const ANIMATION_MS = 180;
const DEFAULT_PANEL_SIZE = { width: 520, height: 680 };
const MIN_PANEL_SIZE = { width: 380, height: 460 };
const DESKTOP_PANEL_MARGIN = 24;
const DESKTOP_PANEL_BOTTOM = 92;

type PanelSize = typeof DEFAULT_PANEL_SIZE;
type PanelState = "opening" | "open" | "closed";

function clamp(value: number, min: number, max: number): number {
  return Math.min(Math.max(value, min), max);
}

function clampPanelSize(size: PanelSize): PanelSize {
  if (typeof window === "undefined") {
    return size;
  }

  const maxWidth = Math.max(MIN_PANEL_SIZE.width, window.innerWidth - DESKTOP_PANEL_MARGIN * 2);
  const maxHeight = Math.max(MIN_PANEL_SIZE.height, window.innerHeight - DESKTOP_PANEL_BOTTOM - DESKTOP_PANEL_MARGIN);

  return {
    width: clamp(size.width, MIN_PANEL_SIZE.width, maxWidth),
    height: clamp(size.height, MIN_PANEL_SIZE.height, maxHeight),
  };
}

export function DsAssistant({ dashboard }: { dashboard: DashboardState }) {
  const [opened, setOpened] = useState(false);
  const [renderPanel, setRenderPanel] = useState(false);
  const [panelState, setPanelState] = useState<PanelState>("closed");
  const [draft, setDraft] = useState("");
  const [panelSize, setPanelSize] = useState<PanelSize>(() => clampPanelSize(DEFAULT_PANEL_SIZE));
  const closeTimerRef = useRef<number | null>(null);
  const animationFrameRef = useRef<number | null>(null);
  const resizeRef = useRef<{ x: number; y: number; width: number; height: number } | null>(null);

  const clearCloseTimer = useCallback(() => {
    if (closeTimerRef.current !== null) {
      window.clearTimeout(closeTimerRef.current);
      closeTimerRef.current = null;
    }
  }, []);

  const openPanel = useCallback(() => {
    clearCloseTimer();
    if (animationFrameRef.current !== null) {
      window.cancelAnimationFrame(animationFrameRef.current);
    }
    setRenderPanel(true);
    setOpened(true);
    setPanelState("opening");
    animationFrameRef.current = window.requestAnimationFrame(() => {
      setPanelState("open");
      animationFrameRef.current = null;
    });
  }, [clearCloseTimer]);

  const closePanel = useCallback(() => {
    clearCloseTimer();
    if (animationFrameRef.current !== null) {
      window.cancelAnimationFrame(animationFrameRef.current);
      animationFrameRef.current = null;
    }
    setOpened(false);
    setPanelState("closed");
    closeTimerRef.current = window.setTimeout(() => {
      setRenderPanel(false);
      closeTimerRef.current = null;
    }, ANIMATION_MS);
  }, [clearCloseTimer]);

  const togglePanel = () => {
    if (opened) {
      closePanel();
    } else {
      openPanel();
    }
  };

  const resizePanel = useCallback((event: PointerEvent) => {
    const resizeStart = resizeRef.current;
    if (!resizeStart) {
      return;
    }

    setPanelSize(clampPanelSize({
      width: resizeStart.width + resizeStart.x - event.clientX,
      height: resizeStart.height + resizeStart.y - event.clientY,
    }));
  }, []);

  const stopResize = useCallback(() => {
    resizeRef.current = null;
    window.removeEventListener("pointermove", resizePanel);
    window.removeEventListener("pointerup", stopResize);
  }, [resizePanel]);

  const startResize = (event: ReactPointerEvent<HTMLButtonElement>) => {
    event.preventDefault();
    resizeRef.current = {
      x: event.clientX,
      y: event.clientY,
      width: panelSize.width,
      height: panelSize.height,
    };
    window.addEventListener("pointermove", resizePanel);
    window.addEventListener("pointerup", stopResize);
  };

  useEffect(() => {
    if (!opened) {
      return undefined;
    }

    const closeOnEscape = (event: KeyboardEvent) => {
      if (event.key === "Escape") {
        closePanel();
      }
    };

    document.addEventListener("keydown", closeOnEscape);
    return () => document.removeEventListener("keydown", closeOnEscape);
  }, [closePanel, opened]);

  useEffect(() => {
    return () => {
      clearCloseTimer();
      if (animationFrameRef.current !== null) {
        window.cancelAnimationFrame(animationFrameRef.current);
      }
      window.removeEventListener("pointermove", resizePanel);
      window.removeEventListener("pointerup", stopResize);
    };
  }, [clearCloseTimer, resizePanel, stopResize]);

  return (
    <>
      {renderPanel && (
        <aside
          id="ds-assistant-panel"
          className="assistant-floating"
          data-state={panelState}
          role="dialog"
          aria-label={dashboard.t("dsAssistantDialog")}
          style={{ width: panelSize.width, height: panelSize.height }}
        >
          <button
            type="button"
            className="assistant-resize-grip"
            aria-label={dashboard.t("resizeDsAssistant")}
            onPointerDown={startResize}
          >
            <Maximize2 size={15} />
          </button>
          <Group className="assistant-floating-header" justify="space-between" align="center" wrap="nowrap">
            <div>
              <Text className="eyebrow">{dashboard.insight.provider}</Text>
              <Title order={2}>{dashboard.t("dsAssistant")}</Title>
            </div>
            <ActionIcon
              variant="subtle"
              color="gray"
              aria-label={dashboard.t("closeDsAssistant")}
              onClick={closePanel}
            >
              <X size={18} />
            </ActionIcon>
          </Group>
          <div className="assistant-floating-body">
            <AiPanel dashboard={dashboard} framed={false} />
            <ChatPanel
              dashboard={dashboard}
              framed={false}
              draft={draft}
              onDraftChange={setDraft}
              scrollHeight={250}
            />
          </div>
        </aside>
      )}

      <button
        type="button"
        className="assistant-launcher"
        aria-label={opened ? dashboard.t("collapseDsAssistant") : dashboard.t("openDsAssistant")}
        aria-controls={renderPanel ? "ds-assistant-panel" : undefined}
        aria-expanded={opened}
        onClick={togglePanel}
      >
        <MessageCircle size={20} />
        <span>{dashboard.t("dsAssistant")}</span>
        <span className={`assistant-launcher-dot risk-${dashboard.snapshot.riskLevel}`} aria-hidden="true" />
      </button>
    </>
  );
}
