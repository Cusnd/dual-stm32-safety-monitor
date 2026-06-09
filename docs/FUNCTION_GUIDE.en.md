# Function Guide

[README](../README.md) | [Chinese](FUNCTION_GUIDE.zh-CN.md) | [Detailed design](FUNCTION_DESIGN_WALKTHROUGH.en.md) | [Project structure](PROJECT_STRUCTURE.en.md)

This guide explains the important functions and classes in the current C++ backend. Start from `Core/Src/main.cpp`, then follow the role selected by `APP_NODE_ROLE`.

## Reading Order

| Step | Code | Why it matters |
|---|---|---|
| 1 | `main()` | Initializes HAL, GPIO, timing, debug USART1, and board-link USART3. |
| 2 | `SensorNode::run()` | Shows how Board A samples sensors and sends one protocol v2 frame per second. |
| 3 | `FrameCodec` and `FrameStreamDecoder` | Explains the binary frame layout and monitor-side resynchronization. |
| 4 | `MonitorNode::run()` | Shows Board B scheduling: receive, buttons, flash processing, alarm, display, logging. |
| 5 | `AlarmEvaluator` and `DisplayFormatter` | Keeps alarm logic and OLED text formatting separate from the monitor loop. |

## Startup And Role Selection

| Function or class | Responsibility |
|---|---|
| `main()` | Selects `SensorNode` or `MonitorNode` at compile time through `APP_NODE_ROLE`. |
| `SystemClock_Config()` | Configures 8 MHz HSE x 9 as 72 MHz SYSCLK, with APB1 at 36 MHz. |
| `MX_GPIO_Init()` | Initializes board-level K1/K2 inputs from CubeMX-style generated code. |
| `hal::initDwtDelay()` | Enables DWT-based microsecond delays for DHT11 and software I2C timing. |
| `hal::initDebugUsart1()` | Sets USART1 `PA9/PA10` to `115200 8N1` for logs and JSON Lines. |
| `hal::initNodeUsart3(enable_rx_interrupt)` | Sets USART3 `PB10/PB11`; the monitor enables RX interrupt and ring-buffer input. |

## Sensor Node

| Function or class | Responsibility |
|---|---|
| `SensorNode::init()` | Initializes Board A GPIO, ADC1, DHT11 state, filters, and sequence counter. |
| `SensorNode::run()` | Samples MQ135, MQ2, rain, thermistor AO/DO, flame DO, and DHT11; fills `SensorFrame`; sends and logs it. |
| `SensorNode::filter()` | Applies a simple 3:1 smoothing filter to analog channels after the first valid sample. |
| `SensorNode::thermistorAdcToC10()` | Converts thermistor ADC to 0.1 deg C using the local lookup/interpolation table and reports ADC faults. |
| `SensorNode::sendFrame()` | Calls `FrameCodec::encode()` and transmits the encoded frame over USART3. |
| `Dht11::read()` | Performs the DHT11 transaction and verifies checksum before updating temperature and humidity. |
| `hal::initAdc1()` / `hal::readAdc1Channel()` | Configure ADC1 and perform blocking single-channel 12-bit reads. |

## Protocol And Stream Decoding

| Function or class | Responsibility |
|---|---|
| `SensorFrame` | Shared in-memory structure for temperature, humidity, MQ values, rain, thermistor, flame, sequence, and status bits. |
| `FrameCodec::encode()` | Converts `SensorFrame` to the 22-byte v2 wire format. |
| `FrameCodec::decode()` | Checks `AA 55`, length, version, checksum, then restores a `SensorFrame`. |
| `FrameCodec::checksum()` | Adds bytes and returns the low 8 bits. |
| `FrameStreamDecoder::push()` | Accepts one byte at a time, finds frame boundaries, reports `FrameReady`, `BadFrame`, or `NeedMore`, and resynchronizes after noise. |

Protocol v2 layout:

```text
AA 55 LEN VER TEMP HUMI MQ135 MQ2 RAIN THERM_ADC THERM_C10 FLAME RAIN_WET THERM_HOT SEQ STATUS CHECKSUM
```

`STATUS` bits are `bit0=DHT error`, `bit1=thermistor DO hot`, `bit2=rain wet`, and `bit3=thermistor ADC fault`.

## Monitor Node

| Function or class | Responsibility |
|---|---|
| `MonitorNode::init()` | Resets monitor state, initializes buzzer, PB0/PB1 threshold keys, OLED bus/controller, W25Q64, and prints boot status. |
| `MonitorNode::run()` | Cooperative loop: process RX, scan buttons, advance flash task, evaluate alarm, update outputs, refresh OLED, and schedule flash logs. |
| `MonitorNode::processRx()` | Pulls USART3 bytes from `hal::readUsartByte()`, feeds `FrameStreamDecoder`, updates `latest_frame_`, and prints JSON schema v2. |
| `MonitorNode::updateButtons()` | K1 toggles OLED page; K2 short press mutes the buzzer for 60 s; PB0 selects MQ135/MQ2/rain/thermistor; PB1 cycles the selected threshold through five levels. |
| `MonitorNode::pressedEdge()` | Debounces the active-low external threshold keys before generating a single press edge. |
| `thresholdsFromLevels()` | Builds the active `AlarmThresholds` from four independent 0..4 sensor levels. |
| `compatibleThresholdProfile()` | Keeps the legacy JSON `thresholdProfile` field: `0` default, `1` old sensitive, `2` old loose, `255` mixed/custom. |
| `MonitorNode::updateAlarm()` | Drives buzzer patterns: fast danger beep, slow node-lost beep, muted silence, warning/normal silence. |
| `MonitorNode::updateDisplay()` | Uses `formatMonitorDisplay()` and writes four OLED lines. |
| `MonitorNode::printFrontendJson()` | Emits one browser-friendly JSON Line with schema v2 fields, per-sensor threshold levels, actual threshold values, alarm state text, and the legacy `externalRgb` placeholder. |

## Alarm, Display, And Board I/O

| Function or class | Responsibility |
|---|---|
| `evaluateAlarm()` | Computes waiting, lost, danger, warn, and muted flags from the latest frame, timeouts, mute window, and active per-sensor thresholds. |
| `alarmStateString()` | Converts `AlarmState` to JSON strings: `normal`, `warn`, `danger`, `waiting`, or `node_lost`. |
| `formatMonitorDisplay()` | Formats page 0 live readings and page 1 selected threshold sensor, level `1/5..5/5`, active values, and flash state. |
| `Buzzer::init()` / `Buzzer::set()` | Configure and drive the active-high buzzer on `PB8`. |
| `Buttons::init()` | Configures external threshold keys on `PB0/PB1` as internal pull-up inputs, active-low to GND. |
| `Buttons::key1Pressed()` / `Buttons::key2Pressed()` | Read active-high K1/K2 board keys. |
| `Buttons::thresholdSelectPressed()` / `Buttons::thresholdLevelPressed()` | Read active-low PB0/PB1 threshold keys. |
| `OledDisplay::initBus()` / `initController()` / `clear()` / `printLine()` | Small SSD1306 software-I2C driver for local monitor display. |

## Optional Flash Logger

| Function or class | Responsibility |
|---|---|
| `W25q64FlashLogger::init()` | Calls `MX_SPI2_Init()`, uses HAL SPI to read JEDEC ID, detects compatible W25Q64 parts, and restores cursor metadata. |
| `W25q64FlashLogger::logFrame()` | Builds one pending 32-byte v2 record with frame data, alarm state, tick, four packed threshold levels plus mute bit, record count, and CRC. |
| `W25q64FlashLogger::process()` | Non-blocking state machine for sector erase, page program, metadata erase, and metadata program tasks. |
| `W25q64FlashLogger::present()` | Reports whether logging is available. |
| `W25q64FlashLogger::recordCount()` | Returns the cumulative record counter shown on OLED and JSON. |

## Frontend Modules

| Module | Responsibility |
|---|---|
| `frontend/src/parser.ts` | Parses MONITOR JSON Lines, validates schema v1/v2, and exposes localizable parser error codes. |
| `frontend/src/analysis.ts` | Uses live threshold fields when present, falls back to legacy `thresholdProfile`, and builds local risk summaries for dashboard and AI fallback. |
| `frontend/src/aiProvider.ts` | Provides local-rule chat plus DeepSeek direct/proxy providers with localized prompts and fallback replies. |
| `frontend/src/hooks/useDashboard.ts` | Owns Web Serial/replay source selection, history, event log, AI mode, and chat state. |
| `frontend/src/components/TrendChart.tsx` | Renders the ECharts trend view with legend, zoom, sensor selection, and history values. |

## Common Changes

| Change | Main files |
|---|---|
| Add a sensor value | `SensorFrame`, `FrameCodec`, `SensorNode::run()`, `MonitorNode::printFrontendJson()`, frontend parser/docs |
| Change alarm thresholds | Level arrays and `thresholdsFromLevels()` in `App/Config.hpp`, plus frontend fallback/defaults in `frontend/src/analysis.ts` |
| Change OLED text | `formatMonitorDisplay()` |
| Change W25Q64 record layout or SPI2 setup | `W25q64FlashLogger::logFrame()`, `Core/Src/spi.c`, SPI MSP setup, CRC/load metadata policy, docs, and any future export tooling |
