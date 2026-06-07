export class WebSerialSource {
  constructor({ onLine, onStatus, onError }) {
    this.onLine = onLine;
    this.onStatus = onStatus;
    this.onError = onError;
    this.port = null;
    this.reader = null;
    this.keepReading = false;
    this.buffer = "";
  }

  static isSupported() {
    return typeof navigator !== "undefined" && "serial" in navigator;
  }

  async connect() {
    if (!WebSerialSource.isSupported()) {
      throw new Error("Web Serial is not supported by this browser");
    }

    this.port = await navigator.serial.requestPort();
    await this.port.open({
      baudRate: 115200,
      dataBits: 8,
      stopBits: 1,
      parity: "none",
      flowControl: "none",
    });

    this.keepReading = true;
    this.onStatus?.("connected");
    this.readLoop();
  }

  async disconnect() {
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

  async readLoop() {
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

  pushText(chunk) {
    this.buffer += chunk;
    const lines = this.buffer.split(/\r?\n/);
    this.buffer = lines.pop() ?? "";
    for (const line of lines) {
      this.onLine?.(line);
    }
  }
}
