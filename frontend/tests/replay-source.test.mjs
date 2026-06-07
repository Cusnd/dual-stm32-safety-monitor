import assert from "node:assert/strict";
import { test } from "node:test";
import { ReplaySerialSource } from "../src/replaySerial.js";

function manualTimer() {
  let nextId = 0;
  const tasks = new Map();
  return {
    api: {
      setTimeout(callback) {
        nextId += 1;
        tasks.set(nextId, callback);
        return nextId;
      },
      clearTimeout(id) {
        tasks.delete(id);
      },
    },
    runNext() {
      const entry = tasks.entries().next().value;
      if (!entry) {
        return false;
      }
      const [id, callback] = entry;
      tasks.delete(id);
      callback();
      return true;
    },
    count() {
      return tasks.size;
    },
  };
}

test("replays serial lines and stops when loop is disabled", async () => {
  const timer = manualTimer();
  const lines = [];
  const statuses = [];
  const source = new ReplaySerialSource({
    text: "first\nsecond\n",
    loop: false,
    timerApi: timer.api,
    onLine: (line) => lines.push(line),
    onStatus: (status) => statuses.push(status),
  });

  await source.connect();
  assert.deepEqual(statuses, ["connected"]);
  assert.equal(timer.count(), 1);

  assert.equal(timer.runNext(), true);
  assert.equal(timer.runNext(), true);

  assert.deepEqual(lines, ["first", "second"]);
  assert.deepEqual(statuses, ["connected", "disconnected"]);
  assert.equal(timer.count(), 0);
});

test("can loop simulated serial output", async () => {
  const timer = manualTimer();
  const lines = [];
  const statuses = [];
  const source = new ReplaySerialSource({
    text: "a\nb\n",
    loop: true,
    timerApi: timer.api,
    onLine: (line) => lines.push(line),
    onStatus: (status) => statuses.push(status),
  });

  await source.connect();
  timer.runNext();
  timer.runNext();
  timer.runNext();

  assert.deepEqual(lines, ["a", "b", "a"]);
  assert.deepEqual(statuses, ["connected", "loop"]);
});

test("disconnect clears pending replay timer", async () => {
  const timer = manualTimer();
  const statuses = [];
  const source = new ReplaySerialSource({
    text: "a\nb\n",
    loop: true,
    timerApi: timer.api,
    onLine: () => {},
    onStatus: (status) => statuses.push(status),
  });

  await source.connect();
  assert.equal(timer.count(), 1);
  await source.disconnect();

  assert.deepEqual(statuses, ["connected", "disconnected"]);
  assert.equal(timer.count(), 0);
});
