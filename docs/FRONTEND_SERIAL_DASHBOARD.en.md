# Web Serial Frontend Dashboard

[Chinese](FRONTEND_SERIAL_DASHBOARD.zh-CN.md) | [README](../README.md)

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
{"type":"sensor","schemaVersion":2,"seq":31,"tickMs":123456,"tempC":26,"humidityPct":54,"mq135Raw":1020,"mq2Raw":860,"rainRaw":980,"thermRaw":1660,"thermC10":325,"rainWet":0,"thermHot":0,"flame":0,"status":0,"alarm":"normal","thresholdProfile":0,"selectedThresholdSensor":0,"thresholdAirLevel":2,"thresholdSmokeLevel":2,"thresholdRainLevel":2,"thresholdThermLevel":2,"thresholdAirWarn":2200,"thresholdSmokeWarn":1800,"thresholdSmokeDanger":2800,"thresholdRainWet":1400,"thresholdThermWarnC10":450,"thresholdThermDangerC10":700,"mute":0,"flashReady":1,"flashRecords":42,"externalRgb":0}
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
| `thresholdProfile` | Legacy compatibility profile: `0` all defaults, `1` old sensitive, `2` old loose, `255` mixed/custom |
| `selectedThresholdSensor` | Currently selected threshold item: `0=MQ135`, `1=MQ2`, `2=RAIN`, `3=THERM` |
| `thresholdAirLevel`, `thresholdSmokeLevel`, `thresholdRainLevel`, `thresholdThermLevel` | Per-sensor level values `0..4`; OLED and docs show them as `1/5..5/5` |
| `thresholdAirWarn`, `thresholdSmokeWarn`, `thresholdSmokeDanger`, `thresholdRainWet` | Actual active raw ADC threshold values |
| `thresholdThermWarnC10`, `thresholdThermDangerC10` | Actual thermistor thresholds in 0.1 deg C |
| `mute` | Buzzer mute state |
| `flashReady`, `flashRecords` | W25Q64 availability and cumulative record count |
| `externalRgb` | Legacy placeholder emitted by current firmware; not required by the dashboard and not an active WS2813/RGB output |

The parser accepts v1 and v2. v1 records show missing v2-only values as `--`. The new threshold fields are optional so old logs still load; when present, dashboard analysis uses the actual threshold values, and when absent it falls back to `thresholdProfile`. Extra placeholder fields such as `externalRgb` may be ignored by the frontend.

## Running

```powershell
npm --prefix frontend install
npm --prefix frontend run dev
```

Open `http://localhost:5173`, connect to Board B at `115200 8N1`, or use replay mode.

## Behavior

- Only lines starting with `{` are parsed as JSON; `[MONITOR]` and `[SENSOR]` logs are ignored.
- Dashboard cards show temperature, humidity, MQ135, MQ2, rain, thermistor, flame, alarm, and runtime details including selected threshold sensor, levels, and actual threshold values.
- Trend chart keeps the latest 80 valid records with ECharts legend, tooltip, zoom, sensor selection, and recent-value history.
- AI Insights show a DeepSeek direct/local hybrid provider by default; local rules still cover MQ, rain, thermistor, flame, DHT, stale data, and node-lost conditions.
- User Chat uses the DeepSeek API key entered in the page to call the official Chat Completions endpoint directly. If the direct network call fails, the frontend falls back to local safety, alarm, MQ2, rain, thermistor, and troubleshooting answers.

## AI Integration

The default mode calls DeepSeek's official Chat Completions endpoint directly from the browser. The user-entered API key is kept only in this browser tab's `sessionStorage`; it is not written to the repository, URL, a local file, or `localStorage`, and it must be entered again after closing the tab.

```text
frontend DirectDeepSeekProvider
        -> Authorization: Bearer <user-entered key>
        -> POST https://api.deepseek.com/chat/completions
```

The direct request body contains only `model`, prompted `messages` with the sensor snapshot, `stream`, and `temperature`; the API key is never placed in the JSON body. The default model is `deepseek-v4-flash`; the default direct endpoint is `https://api.deepseek.com/chat/completions`.

For public deployments, prefer the proxy mode because any browser-direct approach exposes the current user's key to the browser runtime. Optional proxy flow:

```text
frontend DeepSeekProvider
        -> POST /api/ai/chat
        -> backend proxy with DEEPSEEK_API_KEY
        -> DeepSeek-compatible API
```

Proxy requests include `model`, prompted `messages`, raw `conversation`, compact `snapshot`, `locale`, and `requestType: "chat"`.

Optional runtime configuration:

```html
<script>
  window.SAFETY_MONITOR_CONFIG = {
    ai: {
      mode: "direct",
      directEndpoint: "https://api.deepseek.com/chat/completions",
      proxyEndpoint: "/api/ai/chat",
      model: "deepseek-v4-flash"
    }
  };
</script>
```

Temporary URL overrides are also supported: `?ai=direct`, `?ai=local`, `?ai=proxy`, `?aiDirectEndpoint=https://api.deepseek.com/chat/completions`, `?aiProxyEndpoint=/api/ai/chat`, and `?aiModel=deepseek-v4-flash`. The AI mode button switches between direct and local rules and persists the mode choice in browser local storage.

## Build Relationship

The frontend does not replace the firmware workflow:

```powershell
cmake --preset SensorDebug
cmake --build --preset SensorDebug

cmake --preset MonitorDebug
cmake --build --preset MonitorDebug
```

The `Env-Monitor_sensor.hex` and `Env-Monitor_monitor.hex` names are the current firmware artifact names.
