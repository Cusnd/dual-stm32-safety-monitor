import { describe, expect, it } from "vitest";
import { ReplaySerialSource } from "../src/replaySerial";

function manualTimer() {
  let nextId = 0;
  const tasks = new Map<number, () => void>();
  return {
    api: {
      setTimeout(callback: () => void) {
        nextId += 1;
        tasks.set(nextId, callback);
        return nextId;
      },
      clearTimeout(id: number) {
        tasks.delete(id);
      },
    },
    runNext() {
      const entry = tasks.entries().next().value as [number, () => void] | undefined;
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

describe("ReplaySerialSource", () => {
  it("replays serial lines and stops when loop is disabled", async () => {
    const timer = manualTimer();
    const lines: string[] = [];
    const statuses: string[] = [];
    const source = new ReplaySerialSource({
      text: "first\nsecond\n",
      loop: false,
      timerApi: timer.api,
      onLine: (line) => lines.push(line),
      onStatus: (status) => statuses.push(status),
    });

    await source.connect();
    expect(statuses).toEqual(["connected"]);
    expect(timer.count()).toBe(1);

    expect(timer.runNext()).toBe(true);
    expect(timer.runNext()).toBe(true);

    expect(lines).toEqual(["first", "second"]);
    expect(statuses).toEqual(["connected", "disconnected"]);
    expect(timer.count()).toBe(0);
  });

  it("can loop simulated serial output", async () => {
    const timer = manualTimer();
    const lines: string[] = [];
    const statuses: string[] = [];
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

    expect(lines).toEqual(["a", "b", "a"]);
    expect(statuses).toEqual(["connected", "loop"]);
  });

  it("disconnect clears pending replay timer", async () => {
    const timer = manualTimer();
    const statuses: string[] = [];
    const source = new ReplaySerialSource({
      text: "a\nb\n",
      loop: true,
      timerApi: timer.api,
      onLine: () => {},
      onStatus: (status) => statuses.push(status),
    });

    await source.connect();
    expect(timer.count()).toBe(1);
    await source.disconnect();

    expect(statuses).toEqual(["connected", "disconnected"]);
    expect(timer.count()).toBe(0);
  });
});

