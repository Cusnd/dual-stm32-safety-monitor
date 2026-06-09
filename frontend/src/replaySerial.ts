import type { SerialSourceCallbacks } from "./types";

interface TimerApi {
  setTimeout(callback: () => void, delayMs: number): number | ReturnType<typeof setTimeout>;
  clearTimeout(id: number | ReturnType<typeof setTimeout>): void;
}

export class ReplaySerialSource {
  private onLine?: (line: string) => void;
  private onStatus?: SerialSourceCallbacks["onStatus"];
  private onError?: SerialSourceCallbacks["onError"];
  private intervalMs: number;
  private loop: boolean;
  private sourceUrl: string;
  private text: string | null;
  private timerApi: TimerApi;
  private lines: string[] = [];
  private index = 0;
  private running = false;
  private timerId: number | ReturnType<typeof setTimeout> | null = null;

  constructor({
    onLine,
    onStatus,
    onError,
    intervalMs = 700,
    loop = true,
    sourceUrl = "./fixtures/sample-serial.log",
    text = null,
    timerApi = globalThis,
  }: SerialSourceCallbacks & {
    intervalMs?: number;
    loop?: boolean;
    sourceUrl?: string;
    text?: string | null;
    timerApi?: TimerApi;
  }) {
    this.onLine = onLine;
    this.onStatus = onStatus;
    this.onError = onError;
    this.intervalMs = intervalMs;
    this.loop = loop;
    this.sourceUrl = sourceUrl;
    this.text = text;
    this.timerApi = timerApi;
  }

  async connect(): Promise<void> {
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

  async disconnect(): Promise<void> {
    this.clearTimer();
    if (!this.running) {
      return;
    }
    this.running = false;
    this.onStatus?.("disconnected");
  }

  private async loadText(): Promise<string> {
    const response = await fetch(this.sourceUrl, { cache: "no-store" });
    if (!response.ok) {
      throw new Error(`Replay fixture failed: HTTP ${response.status}`);
    }
    return response.text();
  }

  private scheduleNext(delayMs: number): void {
    this.clearTimer();
    this.timerId = this.timerApi.setTimeout(() => this.emitNext(), delayMs);
  }

  private clearTimer(): void {
    if (this.timerId !== null) {
      this.timerApi.clearTimeout(this.timerId);
      this.timerId = null;
    }
  }

  private emitNext(): void {
    if (!this.running) {
      return;
    }

    const line = this.lines[this.index];
    this.index += 1;
    this.onLine?.(line);

    if (this.index >= this.lines.length) {
      if (!this.loop) {
        void this.disconnect();
        return;
      }
      this.index = 0;
      this.onStatus?.("loop");
    }

    this.scheduleNext(this.intervalMs);
  }
}

