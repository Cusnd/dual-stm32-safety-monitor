# Project Structure

[README](../README.md) | [Chinese](PROJECT_STRUCTURE.zh-CN.md) | [Function guide](FUNCTION_GUIDE.en.md) | [Wiring](../WIRING.md)

This repository builds two STM32F103C8T6 firmware images from one source tree and hosts a Vite React Web Serial dashboard.

## Top-Level Layout

```text
.
├── App/                      C++ application backend
│   ├── Drivers/              DHT11, OLED, buzzer/buttons, W25Q64
│   ├── Hal/                  UART, ADC, DWT delay, RX ring buffer helpers
│   ├── Monitor/              Alarm evaluation and OLED display formatting
│   ├── Protocol/             SensorFrame, FrameCodec, FrameStreamDecoder
│   ├── SensorNode.*          Board A SENSOR role
│   └── MonitorNode.*         Board B MONITOR role
├── Core/                     STM32CubeMX-style startup, HAL entry, GPIO, IRQ glue
├── Drivers/                  STM32 HAL and CMSIS vendor sources
├── cmake/                    ARM toolchain files and ST-LINK/OpenOCD config
├── docs/                     Hardware, build, frontend, function, and design docs
├── frontend/                 Static Web Serial dashboard
├── tests/                    Native protocol/decoder tests
├── CMakeLists.txt            Shared firmware build with role-specific sources
├── CMakePresets.json         CLion and command-line presets
├── README.md                 English overview
├── README.zh-CN.md           Chinese overview
└── WIRING.md                 Bilingual wiring guide
```

## Firmware Source Groups

| Area | Main files | Notes |
|---|---|---|
| Entry and role split | `Core/Src/main.cpp`, `CMakeLists.txt`, `CMakePresets.json` | `APP_NODE_ROLE=SENSOR` builds Board A; `APP_NODE_ROLE=MONITOR` builds Board B. |
| Sensor role | `App/SensorNode.*`, `App/Drivers/Dht11.*` | Samples DHT11, MQ135, MQ2, rain, thermistor, and flame; sends protocol v2 frames. |
| Monitor role | `App/MonitorNode.*`, `App/Monitor/*`, `App/Drivers/OledDisplay.*`, `App/Drivers/BoardIo.*`, `App/Drivers/W25q64FlashLogger.*` | Decodes frames, evaluates alarms, refreshes OLED, drives buzzer/buttons, logs optional flash, emits JSON Lines. |
| Protocol | `App/Protocol/SensorFrame.hpp`, `FrameCodec.*`, `FrameStreamDecoder.*` | Defines wire format and stream recovery after noise or bad frames. |
| Hardware helpers | `App/Hal/Hardware.*`, `App/BoardPins.hpp`, `Core/Src/gpio.c`, `Core/Inc/main.h` | UART, ADC, delay, ring buffer, and reference pin definitions. |

The current CMake source lists do not compile WS2813/RGB driver code. Treat older WS2813 hardware notes as legacy/reference unless firmware support is intentionally restored.

## Build Outputs

```powershell
cmake --preset SensorDebug
cmake --build --preset SensorDebug

cmake --preset MonitorDebug
cmake --build --preset MonitorDebug
```

| Preset | Output |
|---|---|
| `SensorDebug` | `build/SensorDebug/Env-Monitor_sensor.elf/.hex/.bin` |
| `MonitorDebug` | `build/MonitorDebug/Env-Monitor_monitor.elf/.hex/.bin` |
| `SensorRelease` | `build/SensorRelease/Env-Monitor_sensor.elf/.hex/.bin` |
| `MonitorRelease` | `build/MonitorRelease/Env-Monitor_monitor.elf/.hex/.bin` |

ST-LINK helper presets call the CMake targets `stlink_flash`, `stlink_server`, and `stlink_gdb`.

## Frontend Layout

```text
frontend/
├── index.html                Vite root document
├── package.json              Vite, React, Mantine, ECharts, and test scripts
├── vite.config.ts            Vite and Vitest configuration
├── src/
│   ├── App.tsx               Dashboard layout and component composition
│   ├── parser.ts             JSON Lines parser and localized error codes
│   ├── analysis.ts           Local threshold/risk analysis
│   ├── aiProvider.ts         Local and DeepSeek providers
│   ├── hooks/useDashboard.ts Web Serial/replay, history, events, and chat state
│   └── components/           Mantine panels and ECharts trend chart
├── fixtures/sample-serial.log
└── tests/*.{ts,tsx}          Vitest service and React component tests
```

Run the dashboard from the repository root:

```powershell
npm --prefix frontend install
npm --prefix frontend run dev
```

Run frontend tests:

```powershell
cd frontend
npm test
```

## Documentation Layout

| File | Purpose |
|---|---|
| `README.md` / `README.zh-CN.md` | Project overview, build, protocol, and demo checklist. |
| `WIRING.md` | Bilingual single-page wiring and power-on checklist. |
| `docs/FUNCTION_GUIDE.en.md` / `.zh-CN.md` | Function-level guide for reading the backend. |
| `docs/FUNCTION_DESIGN_WALKTHROUGH.en.md` / `.zh-CN.md` | Design-level walkthrough with data flow, alarm logic, flash logging, and frontend schema. |
| `docs/FRONTEND_SERIAL_DASHBOARD.en.md` / `.zh-CN.md` | Browser dashboard usage and JSON schema notes. |
| `docs/hardware/index.en.md` / `.zh-CN.md` | Per-chip and per-module hardware reference index. |
| `docs/presentation/README.en.md` / `.zh-CN.md` | Presentation deck build notes. |

`docs/FUNCTION_GUIDE.md`, `docs/PROJECT_STRUCTURE.md`, and `docs/presentation/README.md` are kept as language selectors for old links.

## Edit Map

| Task | Start here |
|---|---|
| Add or rename a protocol field | `App/Protocol/SensorFrame.hpp`, `FrameCodec.*`, `MonitorNode::printFrontendJson()`, `frontend/src/parser.ts`, docs |
| Tune alarm behavior | `App/Config.hpp`, `App/Monitor/AlarmEvaluator.*`, `frontend/src/analysis.ts` |
| Change OLED pages | `App/Monitor/DisplayFormatter.*` |
| Change buzzer or buttons | `App/Drivers/BoardIo.*`, `MonitorNode::updateButtons()`, `MonitorNode::updateAlarm()` |
| Change flash log behavior | `App/Drivers/W25q64FlashLogger.*` |
| Change dashboard text | `frontend/src/i18n.ts` and React components under `frontend/src/components/` |
