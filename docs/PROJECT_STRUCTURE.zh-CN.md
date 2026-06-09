# 项目结构

[中文 README](../README.zh-CN.md) | [English](PROJECT_STRUCTURE.en.md) | [函数说明](FUNCTION_GUIDE.zh-CN.md) | [接线说明](../WIRING.md)

本仓库用同一套源码构建两个 STM32F103C8T6 固件镜像，并包含一个 Vite React Web Serial 看板。

## 顶层目录

```text
.
├── App/                      C++ 应用后端
│   ├── Drivers/              DHT11、OLED、蜂鸣器/按键、W25Q64
│   ├── Hal/                  UART、ADC、DWT 延时、RX 环形缓冲辅助
│   ├── Monitor/              报警评估和 OLED 显示格式化
│   ├── Protocol/             SensorFrame、FrameCodec、FrameStreamDecoder
│   ├── SensorNode.*          板 A SENSOR 角色
│   └── MonitorNode.*         板 B MONITOR 角色
├── Core/                     STM32CubeMX 风格启动、HAL 入口、GPIO/SPI、IRQ glue
├── Drivers/                  STM32 HAL 和 CMSIS 厂商源码，包含 SPI HAL
├── cmake/                    ARM 工具链文件和 ST-LINK/OpenOCD 配置
├── docs/                     硬件、构建、前端、函数和设计文档
├── frontend/                 Vite React Web Serial 看板
├── tests/                    原生协议/解码器测试
├── CMakeLists.txt            共享固件构建和按角色选择源码
├── CMakePresets.json         CLion 与命令行 preset
├── README.md                 英文概览
├── README.zh-CN.md           中文概览
└── WIRING.md                 中英双语接线说明
```

## 固件源码分组

| 区域 | 主要文件 | 说明 |
|---|---|---|
| 入口和角色拆分 | `Core/Src/main.cpp`、`CMakeLists.txt`、`CMakePresets.json` | `APP_NODE_ROLE=SENSOR` 构建板 A；`APP_NODE_ROLE=MONITOR` 构建板 B。 |
| 采集角色 | `App/SensorNode.*`、`App/Drivers/Dht11.*` | 采样 DHT11、MQ135、MQ2、雨量、热敏和火焰，并发送协议 v2 帧。 |
| 显示报警角色 | `App/MonitorNode.*`、`App/Monitor/*`、`App/Drivers/OledDisplay.*`、`App/Drivers/BoardIo.*`、`App/Drivers/W25q64FlashLogger.*` | 解码数据帧、评估报警、刷新 OLED、驱动蜂鸣器/按键、可选记录 Flash、输出 JSON Lines。 |
| 协议 | `App/Protocol/SensorFrame.hpp`、`FrameCodec.*`、`FrameStreamDecoder.*` | 定义线缆格式，以及噪声或坏帧后的流式恢复。 |
| 硬件辅助 | `App/Hal/Hardware.*`、`App/BoardPins.hpp`、`Core/Src/gpio.c`、`Core/Src/spi.c`、`Core/Inc/main.h` | UART、ADC、延时、环形缓冲、SPI2 初始化和参考引脚定义。 |

当前 CMake 源码列表不再编译 WS2813/RGB 驱动。旧 WS2813 硬件资料只应视为 legacy/reference，除非明确恢复固件支持。

## 构建产物

```powershell
cmake --preset SensorDebug
cmake --build --preset SensorDebug

cmake --preset MonitorDebug
cmake --build --preset MonitorDebug
```

| Preset | 输出 |
|---|---|
| `SensorDebug` | `build/SensorDebug/Env-Monitor_sensor.elf/.hex/.bin` |
| `MonitorDebug` | `build/MonitorDebug/Env-Monitor_monitor.elf/.hex/.bin` |
| `SensorRelease` | `build/SensorRelease/Env-Monitor_sensor.elf/.hex/.bin` |
| `MonitorRelease` | `build/MonitorRelease/Env-Monitor_monitor.elf/.hex/.bin` |

ST-LINK 辅助 preset 会调用 CMake targets：`stlink_flash`、`stlink_server` 和 `stlink_gdb`。

## 前端结构

```text
frontend/
├── index.html                Vite 根文档
├── package.json              Vite、React、Mantine、ECharts 和测试脚本
├── vite.config.ts            Vite 与 Vitest 配置
├── src/
│   ├── App.tsx               看板布局与组件组合
│   ├── parser.ts             JSON Lines parser 和本地化错误码
│   ├── analysis.ts           本地阈值/风险分析
│   ├── aiProvider.ts         本地与 DeepSeek providers
│   ├── hooks/useDashboard.ts Web Serial/回放、历史、事件和聊天状态
│   └── components/           Mantine 面板和 ECharts 趋势图
├── fixtures/sample-serial.log
└── tests/*.{ts,tsx}          Vitest service 与 React 组件测试
```

从仓库根目录运行看板：

```powershell
npm --prefix frontend install
npm --prefix frontend run dev
```

运行前端测试：

```powershell
cd frontend
npm test
```

## 文档结构

| 文件 | 用途 |
|---|---|
| `README.md` / `README.zh-CN.md` | 项目概览、构建、协议和演示检查表。 |
| `WIRING.md` | 单页中英双语接线和上电检查表。 |
| `docs/FUNCTION_GUIDE.en.md` / `.zh-CN.md` | 后端函数级阅读指南。 |
| `docs/FUNCTION_DESIGN_WALKTHROUGH.en.md` / `.zh-CN.md` | 设计级 walkthrough，包含数据流、报警逻辑、Flash 记录和前端 schema。 |
| `docs/FRONTEND_SERIAL_DASHBOARD.en.md` / `.zh-CN.md` | 浏览器看板用法和 JSON schema 说明。 |
| `docs/hardware/index.en.md` / `.zh-CN.md` | 芯片和模块硬件资料入口。 |
| `docs/presentation/README.en.md` / `.zh-CN.md` | 展示文稿构建说明。 |

`docs/FUNCTION_GUIDE.md`、`docs/PROJECT_STRUCTURE.md` 和 `docs/presentation/README.md` 保留为旧链接的语言选择页。

## 修改入口

| 任务 | 从这里开始 |
|---|---|
| 添加或重命名协议字段 | `App/Protocol/SensorFrame.hpp`、`FrameCodec.*`、`MonitorNode::printFrontendJson()`、`frontend/src/parser.ts`、文档 |
| 调整报警行为 | `App/Config.hpp`、`App/Monitor/AlarmEvaluator.*`、`frontend/src/analysis.ts` |
| 修改 OLED 页面 | `App/Monitor/DisplayFormatter.*` |
| 修改蜂鸣器或按键 | `App/Drivers/BoardIo.*`、`MonitorNode::updateButtons()`、`MonitorNode::updateAlarm()` |
| 修改 Flash 日志行为 | `App/Drivers/W25q64FlashLogger.*`、`Core/Src/spi.c`、`Core/Src/stm32f1xx_hal_msp.c` |
| 修改看板文本 | `frontend/src/i18n.ts` 和 `frontend/src/components/` 下的 React 组件 |
