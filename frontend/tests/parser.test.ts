import { readFile } from "node:fs/promises";
import { resolve } from "node:path";
import { describe, expect, it } from "vitest";
import { alarmLabel, parseSerialLine, parseSerialText, parserErrorLabel } from "../src/parser";

describe("serial parser", () => {
  it("ignores human readable monitor logs", () => {
    const result = parseSerialLine("[MONITOR] rx seq=1 t=25 h=50");
    expect(result.kind).toBe("ignored");
  });

  it("parses valid JSON Lines sensor records", () => {
    const result = parseSerialLine(
      '{"type":"sensor","seq":31,"tickMs":123456,"tempC":26,"humidityPct":54,"mq135Raw":1020,"mq2Raw":860,"flame":0,"status":0,"alarm":"normal","thresholdProfile":0,"mute":0,"flashReady":1}',
    );

    expect(result.kind).toBe("sensor");
    if (result.kind !== "sensor") {
      return;
    }
    expect(result.data.schemaVersion).toBe(1);
    expect(result.data.seq).toBe(31);
    expect(result.data.tempC).toBe(26);
    expect(result.data.rainRaw).toBeNull();
    expect(result.data.alarm).toBe("normal");
  });

  it("parses v2 rain, thermistor, and flash count fields", () => {
    const result = parseSerialLine(
      '{"type":"sensor","schemaVersion":2,"seq":32,"tickMs":124456,"tempC":27,"humidityPct":55,"mq135Raw":1100,"mq2Raw":900,"rainRaw":1580,"thermRaw":1460,"thermC10":452,"rainWet":1,"thermHot":0,"flame":0,"status":4,"alarm":"warn","thresholdProfile":255,"selectedThresholdSensor":1,"thresholdAirLevel":2,"thresholdSmokeLevel":0,"thresholdRainLevel":2,"thresholdThermLevel":2,"thresholdAirWarn":2200,"thresholdSmokeWarn":1200,"thresholdSmokeDanger":2200,"thresholdRainWet":1400,"thresholdThermWarnC10":450,"thresholdThermDangerC10":700,"mute":0,"flashReady":1,"flashRecords":42}',
    );

    expect(result.kind).toBe("sensor");
    if (result.kind !== "sensor") {
      return;
    }
    expect(result.data.schemaVersion).toBe(2);
    expect(result.data.rainRaw).toBe(1580);
    expect(result.data.thermC10).toBe(452);
    expect(result.data.thresholdProfile).toBe(255);
    expect(result.data.selectedThresholdSensor).toBe(1);
    expect(result.data.thresholdSmokeDanger).toBe(2200);
    expect(result.data.flashRecords).toBe(42);
  });

  it("reports missing fields", () => {
    const result = parseSerialLine(
      '{"type":"sensor","seq":31,"tickMs":123456,"tempC":26,"humidityPct":54,"mq135Raw":1020,"mq2Raw":860,"flame":0,"status":0,"alarm":"normal","thresholdProfile":0,"mute":0}',
    );

    expect(result.kind).toBe("error");
    if (result.kind === "error") {
      expect(result.error).toMatch(/missing field: flashReady/);
    }
  });

  it("reports missing v2 extension fields", () => {
    const result = parseSerialLine(
      '{"type":"sensor","schemaVersion":2,"seq":31,"tickMs":123456,"tempC":26,"humidityPct":54,"mq135Raw":1020,"mq2Raw":860,"flame":0,"status":0,"alarm":"normal","thresholdProfile":0,"mute":0,"flashReady":1}',
    );

    expect(result.kind).toBe("error");
    if (result.kind === "error") {
      expect(result.error).toMatch(/missing field: rainRaw/);
    }
  });

  it("reports invalid JSON", () => {
    const result = parseSerialLine('{"type":"sensor",');
    expect(result.kind).toBe("error");
  });

  it("validates alarm states and labels", () => {
    const result = parseSerialLine(
      '{"type":"sensor","seq":31,"tickMs":123456,"tempC":26,"humidityPct":54,"mq135Raw":1020,"mq2Raw":860,"flame":0,"status":0,"alarm":"panic","thresholdProfile":0,"mute":0,"flashReady":1}',
    );

    expect(result.kind).toBe("error");
    if (result.kind === "error") {
      expect(result.error).toMatch(/alarm/);
    }
    expect(alarmLabel("danger", "zh-CN")).toBe("危险");
    expect(alarmLabel("node_lost", "en")).toBe("Node lost");
    expect(parserErrorLabel("missing field: rainRaw", "zh-CN")).toBe("缺少字段: rainRaw");
    expect(parserErrorLabel("mq2Raw must be an integer", "en")).toBe("mq2Raw must be an integer");
  });

  it("parses mixed sample serial fixture", async () => {
    const text = await readFile(resolve(process.cwd(), "fixtures", "sample-serial.log"), "utf8");
    const results = parseSerialText(text);
    const sensors = results.filter((result) => result.kind === "sensor");

    expect(sensors).toHaveLength(5);
    expect(sensors.map((result) => result.kind === "sensor" ? result.data.alarm : "")).toEqual([
      "normal",
      "normal",
      "warn",
      "danger",
      "warn",
    ]);
  });
});
