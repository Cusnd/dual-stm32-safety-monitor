import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import { test } from "node:test";
import { parseSerialLine, parseSerialText, alarmLabel } from "../src/parser.js";

test("ignores human readable monitor logs", () => {
  const result = parseSerialLine("[MONITOR] rx seq=1 t=25 h=50");
  assert.equal(result.kind, "ignored");
});

test("parses valid JSON Lines sensor records", () => {
  const result = parseSerialLine(
    '{"type":"sensor","seq":31,"tickMs":123456,"tempC":26,"humidityPct":54,"mq135Raw":1020,"mq2Raw":860,"flame":0,"status":0,"alarm":"normal","thresholdProfile":0,"mute":0,"flashReady":1}',
  );

  assert.equal(result.kind, "sensor");
  assert.equal(result.data.seq, 31);
  assert.equal(result.data.tempC, 26);
  assert.equal(result.data.alarm, "normal");
});

test("reports missing fields", () => {
  const result = parseSerialLine(
    '{"type":"sensor","seq":31,"tickMs":123456,"tempC":26,"humidityPct":54,"mq135Raw":1020,"mq2Raw":860,"flame":0,"status":0,"alarm":"normal","thresholdProfile":0,"mute":0}',
  );

  assert.equal(result.kind, "error");
  assert.match(result.error, /missing field: flashReady/);
});

test("reports invalid JSON", () => {
  const result = parseSerialLine('{"type":"sensor",');
  assert.equal(result.kind, "error");
});

test("validates alarm states and labels", () => {
  const result = parseSerialLine(
    '{"type":"sensor","seq":31,"tickMs":123456,"tempC":26,"humidityPct":54,"mq135Raw":1020,"mq2Raw":860,"flame":0,"status":0,"alarm":"panic","thresholdProfile":0,"mute":0,"flashReady":1}',
  );

  assert.equal(result.kind, "error");
  assert.match(result.error, /alarm/);
  assert.equal(alarmLabel("danger", "zh-CN"), "危险");
  assert.equal(alarmLabel("node_lost", "en"), "Node lost");
});

test("parses mixed sample serial fixture", async () => {
  const text = await readFile(new URL("../fixtures/sample-serial.log", import.meta.url), "utf8");
  const results = parseSerialText(text);
  const sensors = results.filter((result) => result.kind === "sensor");

  assert.equal(sensors.length, 5);
  assert.deepEqual(
    sensors.map((result) => result.data.alarm),
    ["normal", "normal", "warn", "danger", "warn"],
  );
});
