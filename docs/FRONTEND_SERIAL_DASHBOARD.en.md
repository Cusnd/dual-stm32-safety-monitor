# Web Serial Frontend Dashboard

[中文](FRONTEND_SERIAL_DASHBOARD.zh-CN.md) | [README](../README.md)

## Purpose

`frontend/` is the browser dashboard for Board B. Board B receives binary v2 sensor frames over USART3 and prints JSON Lines on USART1. A PC reads the USB-UART serial port through Chrome or Edge Web Serial.

```text
Board A SENSOR --USART3 PB10/PB11--> Board B MONITOR --USART1 PA9/PA10 + USB-UART--> Browser Web Serial
```

## Firmware Output

After Board B decodes a valid v2 frame, it prints a human-readable log:

```text
[MONITOR] rx v2 seq=31 t=26 h=54 mq135=1020 mq2=860 rain=980 therm=32.5C flame=0 status=0x00
```

The next line is machine-readable JSON:

```json
{"type":"sensor","schemaVersion":2,"seq":31,"tickMs":123456,"tempC":26,"humidityPct":54,"mq135Raw":1020,"mq2Raw":860,"rainRaw":980,"thermRaw":1660,"thermC10":325,"rainWet":0,"thermHot":0,"flame":0,"status":0,"alarm":"normal","thresholdProfile":0,"mute":0,"flashReady":1,"flashRecords":42,"externalRgb":1}
```

| Field | Meaning |
|---|---|
| `schemaVersion` | JSON schema version; missing means old v1, current firmware emits v2 |
| `seq`, `tickMs` | Sensor sequence and Board B tick |
| `tempC`, `humidityPct` | DHT11 values |
| `mq135Raw`, `mq2Raw` | Raw 12-bit ADC values |
| `rainRaw`, `rainWet` | Rain ADC and wet state |
| `thermRaw`, `thermC10`, `thermHot` | Thermistor ADC, 0.1 deg C temperature, and DO high-temperature state |
| `flame` | `1` when flame is detected |
| `status` | `bit0=DHT error`, `bit1=thermistor DO hot`, `bit2=rain wet`, `bit3=thermistor ADC fault` |
| `alarm` | `normal`, `warn`, `danger`, or `node_lost` |
| `thresholdProfile`, `mute` | Active threshold profile and buzzer mute state |
| `flashReady`, `flashRecords` | W25Q64 availability and cumulative record count |
| `externalRgb` | Whether the WS2813E driver initialized |

The parser accepts v1 and v2. v1 records show missing v2-only values as `--`.

## Running

```powershell
python -m http.server 5173 -d frontend
```

Open `http://localhost:5173`, connect to Board B at `115200 8N1`, or use replay mode.

## Behavior

- Only lines starting with `{` are parsed as JSON; `[MONITOR]` and `[SENSOR]` logs are ignored.
- Dashboard cards show temperature, humidity, MQ135, MQ2, rain, thermistor, flame, and alarm.
- Trend chart keeps the latest 80 valid records and draws T/H/MQ2/MQ135/RAIN/thermistor series.
- AI Insights show a DeepSeek/local hybrid provider by default; local rules still cover MQ, rain, thermistor, flame, DHT, stale data, and node-lost conditions.
- User Chat sends the current snapshot and recent history to the backend `/api/ai/chat` proxy. If the backend is unavailable, the frontend falls back to local safety, alarm, MQ2, rain, thermistor, and troubleshooting answers.

## AI Integration

The frontend does not call DeepSeek directly and does not store an API key. Keep the DeepSeek key only in a local or cloud backend environment variable. The browser calls the proxy:

```text
frontend DeepSeekProvider
        -> POST /api/ai/chat
        -> backend proxy with DEEPSEEK_API_KEY
        -> DeepSeek-compatible API
```

The default request body includes `model`, prompted `messages`, raw `conversation`, compact `snapshot`, `locale`, and `requestType: "chat"`. The default model is `deepseek-v4-flash`; the default proxy endpoint is `/api/ai/chat`.

Optional runtime configuration:

```html
<script>
  window.SAFETY_MONITOR_CONFIG = {
    ai: {
      mode: "deepseek",
      endpoint: "/api/ai/chat",
      model: "deepseek-v4-flash"
    }
  };
</script>
```

Temporary URL overrides are also supported: `?ai=local`, `?ai=deepseek`, `?aiEndpoint=/api/ai/chat`, and `?aiModel=deepseek-v4-flash`. The AI mode button switches between DeepSeek and local rules and persists the choice in browser local storage.

## Build Relationship

The frontend does not replace the firmware workflow:

```powershell
cmake --preset SensorDebug
cmake --build --preset SensorDebug

cmake --preset MonitorDebug
cmake --build --preset MonitorDebug
```

The `Env-Monitor_sensor.hex` and `Env-Monitor_monitor.hex` names are the current firmware artifact names.
