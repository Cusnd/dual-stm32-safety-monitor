# Web Serial Frontend Dashboard

[中文](FRONTEND_SERIAL_DASHBOARD.zh-CN.md) | [Back to README](../README.md)

## Purpose

`frontend/` is a browser dashboard for the Board B MONITOR node. Board B still receives Board A binary sensor frames over USART3, and now also prints JSON Lines on the USART1 debug port. The PC sees the on-board CH340C as a serial port, and Chrome or Edge can read it through the Web Serial API.

Data path:

```text
Board A SENSOR --USART3 PB10/PB11--> Board B MONITOR --USART1 PA9/PA10 + CH340C--> Browser Web Serial
```

## Firmware Output

After Board B decodes a valid sensor frame, it keeps the original human-readable log:

```text
[MONITOR] rx seq=31 t=26 h=54 mq135=1020 mq2=860 flame=0 status=0x00
```

The firmware then prints one JSON line:

```json
{"type":"sensor","seq":31,"tickMs":123456,"tempC":26,"humidityPct":54,"mq135Raw":1020,"mq2Raw":860,"flame":0,"status":0,"alarm":"normal","thresholdProfile":0,"mute":0,"flashReady":1}
```

Fields:

| Field | Meaning |
|---|---|
| `type` | Always `sensor` |
| `seq` | Rolling frame sequence number from Board A |
| `tickMs` | Board B `HAL_GetTick()` when the JSON line is printed |
| `tempC` / `humidityPct` | DHT11 temperature and humidity |
| `mq135Raw` / `mq2Raw` | Raw 12-bit ADC values |
| `flame` | `1` when flame is detected |
| `status` | Frame status byte, `bit0 = DHT11 read error` |
| `alarm` | `normal`, `warn`, `danger`, or `node_lost` |
| `thresholdProfile` | Threshold profile selected by K2 long press |
| `mute` | Whether the buzzer is inside the K2 short-press mute window |
| `flashReady` | Whether Board B detected a usable W25Q64 |

## Running The Frontend

From the repository root:

```powershell
python -m http.server 5173 -d frontend
```

Open Chrome or Edge at:

```text
http://localhost:5173
```

Press "Connect serial" and select the CH340C port for Board B. Serial settings are fixed at `115200 8N1`.

## Behavior

- The page parses JSON Lines that start with `{` and ignores ordinary `[MONITOR]` or `[SENSOR]` logs.
- The dashboard shows temperature, humidity, MQ135, MQ2, flame, alarm, threshold profile, mute, and Flash state.
- The trend chart keeps the latest 80 valid sensor records.
- If no valid JSON arrives for more than 3 seconds, the page shows a stale state while keeping the last valid values visible.
- "Start replay" streams `frontend/fixtures/sample-serial.log` line by line through the same parser path used by real Web Serial.
- The AI Insights panel shows risk level, evidence, trend, and recommended action using the local rules provider.
- The User Chat panel answers safety, alarm, MQ2, and troubleshooting questions from the current sensor snapshot.
- Web Serial is mainly available in Chrome and Edge; other browsers show an unsupported-browser state.

## AI Integration Stub

The current frontend does not call DeepSeek directly and does not store an API key. When DeepSeek V4-flash is connected later, use a local or hosted backend proxy such as:

```text
frontend LocalInsightProvider / DeepSeekProvider
        -> POST /api/ai/chat
        -> DeepSeek-compatible API
```

The frontend already has an `AiProvider` interface: `analyze(snapshot)` and `chat({ messages, snapshot, locale })`. A later integration should replace the provider without changing the sensor JSON Lines format or the dashboard layout.

## CLion/CMake Relationship

The frontend does not replace the CLion + CMake workflow. Firmware is still built with the existing presets:

```powershell
cmake --preset MonitorDebug
cmake --build --preset MonitorDebug

cmake --preset SensorDebug
cmake --build --preset SensorDebug
```

The flashing targets are unchanged: `Fire_F103_sensor.hex` goes to Board A, and `Fire_F103_monitor.hex` goes to Board B. The frontend reads only Board B USART1 debug output and does not occupy the USART3 board-to-board link.
