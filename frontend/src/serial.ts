import type { SerialSourceCallbacks } from "./types";

interface SerialPortLike {
  readable: ReadableStream<Uint8Array> | null;
  open(options: {
    baudRate: number;
    dataBits: number;
    stopBits: number;
    parity: "none";
    flowControl: "none";
  }): Promise<void>;
  close(): Promise<void>;
}

interface SerialNavigator extends Navigator {
  serial?: {
    requestPort(): Promise<SerialPortLike>;
  };
}

export class WebSerialSource {
  private onLine?: (line: string) => void;
  private onStatus?: SerialSourceCallbacks["onStatus"];
  private onError?: SerialSourceCallbacks["onError"];
  private port: SerialPortLike | null = null;
  private reader: ReadableStreamDefaultReader<Uint8Array> | null = null;
  private keepReading = false;
  private buffer = "";

  constructor({ onLine, onStatus, onError }: SerialSourceCallbacks) {
    this.onLine = onLine;
    this.onStatus = onStatus;
    this.onError = onError;
  }

  static isSupported(): boolean {
    return typeof navigator !== "undefined" && "serial" in navigator;
  }

  async connect(): Promise<void> {
    if (!WebSerialSource.isSupported()) {
      throw new Error("Web Serial is not supported by this browser");
    }

    const serialNavigator = navigator as SerialNavigator;
    this.port = await serialNavigator.serial!.requestPort();
    await this.port.open({
      baudRate: 115200,
      dataBits: 8,
      stopBits: 1,
      parity: "none",
      flowControl: "none",
    });

    this.keepReading = true;
    this.onStatus?.("connected");
    void this.readLoop();
  }

  async disconnect(): Promise<void> {
    this.keepReading = false;
    if (this.reader) {
      try {
        await this.reader.cancel();
      } catch {
        // Reader cancellation can fail after physical disconnect.
      }
      this.reader.releaseLock();
      this.reader = null;
    }

    if (this.port) {
      try {
        await this.port.close();
      } catch {
        // Closing an already-removed USB serial device is harmless here.
      }
      this.port = null;
    }

    this.onStatus?.("disconnected");
  }

  private async readLoop(): Promise<void> {
    const decoder = new TextDecoder();

    try {
      while (this.keepReading && this.port?.readable) {
        this.reader = this.port.readable.getReader();
        try {
          while (this.keepReading) {
            const { value, done } = await this.reader.read();
            if (done) {
              break;
            }
            this.pushText(decoder.decode(value, { stream: true }));
          }
        } finally {
          this.reader.releaseLock();
          this.reader = null;
        }
      }
    } catch (error) {
      this.onError?.(error);
    } finally {
      this.keepReading = false;
      this.onStatus?.("disconnected");
    }
  }

  pushText(chunk: string): void {
    this.buffer += chunk;
    const lines = this.buffer.split(/\r?\n/);
    this.buffer = lines.pop() ?? "";
    for (const line of lines) {
      this.onLine?.(line);
    }
  }
}

