export class ReplaySerialSource {
  constructor({
    onLine,
    onStatus,
    onError,
    intervalMs = 700,
    loop = true,
    sourceUrl = "./fixtures/sample-serial.log",
    text = null,
    timerApi = globalThis,
  }) {
    this.onLine = onLine;
    this.onStatus = onStatus;
    this.onError = onError;
    this.intervalMs = intervalMs;
    this.loop = loop;
    this.sourceUrl = sourceUrl;
    this.text = text;
    this.timerApi = timerApi;
    this.lines = [];
    this.index = 0;
    this.running = false;
    this.timerId = null;
  }

  async connect() {
    if (this.running) {
      return;
    }

    try {
      const text = this.text ?? await this.loadText();
      this.lines = text.split(/\r?\n/).filter((line) => line.trim().length > 0);
      if (this.lines.length === 0) {
        throw new Error("Replay source has no serial lines");
      }

      this.index = 0;
      this.running = true;
      this.onStatus?.("connected");
      this.scheduleNext(0);
    } catch (error) {
      this.onError?.(error);
    }
  }

  async disconnect() {
    this.clearTimer();
    if (!this.running) {
      return;
    }
    this.running = false;
    this.onStatus?.("disconnected");
  }

  async loadText() {
    const response = await fetch(this.sourceUrl, { cache: "no-store" });
    if (!response.ok) {
      throw new Error(`Replay fixture failed: HTTP ${response.status}`);
    }
    return response.text();
  }

  scheduleNext(delayMs) {
    this.clearTimer();
    this.timerId = this.timerApi.setTimeout(() => this.emitNext(), delayMs);
  }

  clearTimer() {
    if (this.timerId !== null) {
      this.timerApi.clearTimeout(this.timerId);
      this.timerId = null;
    }
  }

  emitNext() {
    if (!this.running) {
      return;
    }

    const line = this.lines[this.index];
    this.index += 1;
    this.onLine?.(line);

    if (this.index >= this.lines.length) {
      if (!this.loop) {
        this.disconnect();
        return;
      }
      this.index = 0;
      this.onStatus?.("loop");
    }

    this.scheduleNext(this.intervalMs);
  }
}
