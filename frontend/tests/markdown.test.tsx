import { render, screen } from "@testing-library/react";
import { describe, expect, it } from "vitest";
import { MarkdownMessage } from "../src/components/MarkdownMessage";

describe("MarkdownMessage", () => {
  it("renders common assistant markdown", () => {
    render(
      <MarkdownMessage
        content={[
          "### 风险结论",
          "**危险**：MQ2 已超过阈值。",
          "",
          "- 检查 `MQ2` 传感器",
          "- 保持通风",
          "",
          "```json",
          "{\"alarm\":\"danger\"}",
          "```",
        ].join("\n")}
      />,
    );

    expect(screen.getByRole("heading", { name: "风险结论" })).toBeInTheDocument();
    expect(screen.getByText("危险")).toBeInTheDocument();
    expect(screen.getByText("MQ2")).toBeInTheDocument();
    expect(screen.getByText(/\{"alarm":"danger"\}/)).toBeInTheDocument();
  });

  it("supports GFM tables and blocks raw html/javascript links", () => {
    const { container } = render(
      <MarkdownMessage
        content={[
          '<img src=x onerror="alert(1)">',
          "[文档](https://example.com/path?a=1&b=2)",
          "[bad](javascript:alert(1))",
          "",
          "| Key | Value |",
          "| --- | --- |",
          "| risk | danger |",
        ].join("\n")}
      />,
    );

    expect(container.querySelector("img")).toBeNull();
    expect(container.querySelector('a[href^="javascript:"]')).toBeNull();
    expect(screen.getByRole("link", { name: "文档" })).toHaveAttribute("href", "https://example.com/path?a=1&b=2");
    expect(screen.getByRole("table")).toBeInTheDocument();
  });
});

