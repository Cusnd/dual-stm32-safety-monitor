import { MantineProvider } from "@mantine/core";
import { Notifications } from "@mantine/notifications";
import { fireEvent, render, screen, waitFor } from "@testing-library/react";
import { afterEach, describe, expect, it, vi } from "vitest";
import App from "../src/App";

vi.mock("echarts-for-react", () => ({
  default: ({ option, onEvents }: { option: any; onEvents?: Record<string, (params: any) => void> }) => (
    <button
      type="button"
      data-testid="echarts-mock"
      data-series-count={option.series?.length ?? 0}
      onClick={() => onEvents?.click?.({ seriesId: "mq2Raw", seriesName: "MQ2" })}
    >
      chart
    </button>
  ),
}));

const SAMPLE = [
  '{"type":"sensor","schemaVersion":2,"seq":1,"tickMs":1000,"tempC":26,"humidityPct":54,"mq135Raw":1020,"mq2Raw":860,"rainRaw":900,"thermRaw":1500,"thermC10":260,"rainWet":0,"thermHot":0,"flame":0,"status":0,"alarm":"normal","thresholdProfile":0,"mute":0,"flashReady":1,"flashRecords":4}',
  '{"type":"sensor","schemaVersion":2,"seq":2,"tickMs":1700,"tempC":27,"humidityPct":55,"mq135Raw":1300,"mq2Raw":1900,"rainRaw":1510,"thermRaw":1600,"thermC10":452,"rainWet":1,"thermHot":0,"flame":0,"status":4,"alarm":"warn","thresholdProfile":0,"mute":0,"flashReady":1,"flashRecords":5}',
].join("\n");

function renderApp(search = "/?ai=local") {
  window.history.pushState({}, "", search);
  return render(
    <MantineProvider>
      <Notifications />
      <App />
    </MantineProvider>,
  );
}

function mockReplayFetch() {
  globalThis.fetch = vi.fn(async () => new Response(SAMPLE, {
    status: 200,
    headers: { "Content-Type": "text/plain" },
  })) as typeof fetch;
}

afterEach(() => {
  vi.restoreAllMocks();
  vi.unstubAllGlobals();
});

describe("React dashboard", () => {
  it("switches visible labels, aria labels, and document state to English", () => {
    renderApp();

    fireEvent.click(screen.getByRole("button", { name: "切换语言" }));

    expect(document.documentElement.lang).toBe("en");
    expect(document.title).toBe("Safety Monitor Dashboard");
    expect(screen.getByRole("heading", { name: "Safety Monitor Dashboard" })).toBeInTheDocument();
    expect(screen.getByLabelText("Live sensor values")).toBeInTheDocument();
  });

  it("streams replay data into metrics, events, and the chart", async () => {
    mockReplayFetch();
    renderApp();

    fireEvent.click(screen.getByRole("button", { name: /开始模拟/ }));

    await waitFor(() => expect(screen.getByText("模拟串口")).toBeInTheDocument());
    expect(await screen.findByText("seq=1 正常")).toBeInTheDocument();
    expect(screen.getAllByText("860").length).toBeGreaterThan(0);
    expect(screen.getByTestId("echarts-mock")).toHaveAttribute("data-series-count", "6");
  });

  it("selects a sensor series from the chart and shows history values", async () => {
    mockReplayFetch();
    renderApp();

    fireEvent.click(screen.getByRole("button", { name: /开始模拟/ }));
    await screen.findByText("seq=1 正常");

    fireEvent.click(screen.getByTestId("echarts-mock"));

    expect(screen.getByText(/选中传感器: MQ2/)).toBeInTheDocument();
    expect(screen.getByText(/历史值: MQ2/)).toBeInTheDocument();
    expect(screen.getByText("860 raw")).toBeInTheDocument();
  });

  it("keeps only the DS launcher visible until it opens the floating panel", () => {
    renderApp();

    expect(screen.getByRole("button", { name: /打开 DS 对话/ })).toBeInTheDocument();
    expect(screen.queryByRole("dialog", { name: "DS 对话浮窗" })).not.toBeInTheDocument();
    expect(screen.queryByRole("heading", { name: "AI 洞察" })).not.toBeInTheDocument();
    expect(screen.queryByPlaceholderText("现在安全吗？")).not.toBeInTheDocument();

    fireEvent.click(screen.getByRole("button", { name: /打开 DS 对话/ }));

    expect(screen.getByRole("dialog", { name: "DS 对话浮窗" })).toBeInTheDocument();
    expect(screen.getByPlaceholderText("现在安全吗？")).toBeInTheDocument();
    expect(screen.getByRole("heading", { name: "AI 洞察" })).toBeInTheDocument();
    expect(screen.getByRole("heading", { name: "用户对话" })).toBeInTheDocument();
  });

  it("closes DS chat with Escape while keeping the launcher available", async () => {
    renderApp();

    fireEvent.click(screen.getByRole("button", { name: /打开 DS 对话/ }));
    expect(screen.getByRole("dialog", { name: "DS 对话浮窗" })).toBeInTheDocument();

    fireEvent.keyDown(document, { key: "Escape" });

    await waitFor(() => expect(screen.queryByRole("dialog", { name: "DS 对话浮窗" })).not.toBeInTheDocument());
    expect(screen.getByRole("button", { name: /打开 DS 对话/ })).toBeInTheDocument();
  });

  it("preserves DS chat messages after closing and reopening the floating panel", async () => {
    renderApp();

    fireEvent.click(screen.getByRole("button", { name: /打开 DS 对话/ }));
    fireEvent.change(screen.getByPlaceholderText("现在安全吗？"), { target: { value: "现在安全吗？" } });
    fireEvent.click(screen.getByRole("button", { name: /发送/ }));
    const reply = await screen.findByText(/现在还没有有效数据/);

    fireEvent.click(screen.getByRole("button", { name: /关闭 DS 对话/ }));
    await waitFor(() => expect(screen.queryByPlaceholderText("现在安全吗？")).not.toBeInTheDocument());

    fireEvent.click(screen.getByRole("button", { name: /打开 DS 对话/ }));

    expect(reply).not.toBeInTheDocument();
    expect(screen.getByText(/现在还没有有效数据/)).toBeInTheDocument();
  });

  it("resizes the DS panel and keeps the session size after reopening", async () => {
    vi.spyOn(window, "innerWidth", "get").mockReturnValue(1200);
    vi.spyOn(window, "innerHeight", "get").mockReturnValue(1000);
    renderApp();

    fireEvent.click(screen.getByRole("button", { name: /打开 DS 对话/ }));
    const panel = screen.getByRole("dialog", { name: "DS 对话浮窗" });
    const grip = screen.getByRole("button", { name: "调整 DS 对话大小" });

    fireEvent.pointerDown(grip, { clientX: 200, clientY: 200 });
    fireEvent.pointerMove(window, { clientX: 160, clientY: 150 });
    fireEvent.pointerUp(window);

    expect(panel).toHaveStyle({ width: "560px", height: "730px" });

    fireEvent.click(screen.getByRole("button", { name: /关闭 DS 对话/ }));
    await waitFor(() => expect(screen.queryByRole("dialog", { name: "DS 对话浮窗" })).not.toBeInTheDocument());

    fireEvent.click(screen.getByRole("button", { name: /打开 DS 对话/ }));

    expect(screen.getByRole("dialog", { name: "DS 对话浮窗" })).toHaveStyle({ width: "560px", height: "730px" });
  });

  it("keeps existing chat message nodes stable while sensor data refreshes", async () => {
    mockReplayFetch();
    renderApp();

    fireEvent.click(screen.getByRole("button", { name: /打开 DS 对话/ }));
    fireEvent.change(screen.getByPlaceholderText("现在安全吗？"), { target: { value: "现在安全吗？" } });
    fireEvent.click(screen.getByRole("button", { name: /发送/ }));
    const reply = await screen.findByText(/现在还没有有效数据/);

    fireEvent.click(screen.getByRole("button", { name: /开始模拟/ }));
    await screen.findByText("seq=1 正常");

    expect(screen.getByText(/现在还没有有效数据/)).toBe(reply);
  });
});
