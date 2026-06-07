# Wiring Guide / 接线说明

[Hardware reference](docs/hardware/index.en.md) / [硬件资料](docs/hardware/index.zh-CN.md)

This guide describes the current reference pin map for a generic STM32F103C8T6 dual-board implementation. If you use another STM32F103C8T6 board, keep the signal roles and electrical limits, or update firmware and docs together.

本文档说明通用 STM32F103C8T6 双板参考实现的当前引脚映射。若使用其他 STM32F103C8T6 板卡，请保持信号角色和电气限制一致；若改脚，需要同步修改固件和文档。

## Board-Porting Checklist / 板卡移植检查表

| Check / 检查项 | Requirement / 要求 |
|---|---|
| MCU supply / MCU 供电 | 3.3 V logic, stable VDDA for ADC / 3.3 V 逻辑电平，VDDA 稳定 |
| ADC inputs / ADC 输入 | Never exceed VDDA; add dividers/clamps for 5 V modules / 不得超过 VDDA；5 V 模块需分压或限幅 |
| Debug UART / 调试串口 | Reference uses USART1 `PA9/PA10`; target board may route this to any USB-UART bridge / 参考实现用 USART1 `PA9/PA10`；目标板可接任意 USB 转串口 |
| Board link / 双板通信 | USART3 `PB10/PB11`, crossed TX/RX, common GND / USART3 `PB10/PB11`，TX/RX 交叉，共地 |
| SWD / 下载调试 | Keep `PA13/PA14` free / 保留 `PA13/PA14` |
| 5 V LED power / 5 V 灯珠供电 | WS2813E uses 5 V power but needs common ground and preferably data level shifting / WS2813E 5 V 供电，但必须共地，数据线建议电平转换 |

## Reserved Or Board-Dependent Pins / 保留或板卡相关引脚

| Pin / 引脚 | Reference use / 参考用途 | Porting note / 移植说明 |
|---|---|---|
| `PA9` | USART1_TX debug/JSON / USART1_TX 调试与 JSON | Connect to USB-UART RX if available / 有 USB 转串口时接其 RX |
| `PA10` | USART1_RX debug / USART1_RX 调试 | Connect to USB-UART TX if available / 有 USB 转串口时接其 TX |
| `PA11/PA12` | USB DM/DP reserved / USB DM/DP 保留 | Do not reuse when USB is populated / 接了 USB 时不复用 |
| `PA13/PA14` | SWD / SWD 下载调试 | Keep free / 保留 |
| `PA1/PA2/PA3` | On-board RGB in reference wiring / 参考板板载 RGB | Active-low assumption is board-specific / 低电平点亮是假设，按目标板确认 |
| `PA0/PC13` | K1/K2 in reference wiring / 参考板 K1/K2 | Active-high assumption is board-specific / 高电平按下是假设，按目标板确认 |

## Board-to-Board USART3 Link / 双板 USART3 连接

| Board A SENSOR / 板 A | Board B MONITOR / 板 B | Note / 说明 |
|---|---|---|
| `PB10 / USART3_TX` | `PB11 / USART3_RX` | Sensor frames to monitor / 采集帧发往显示节点 |
| `PB11 / USART3_RX` | `PB10 / USART3_TX` | Reserved reverse path / 预留反向通道 |
| `GND` | `GND` | Required / 必须共地 |

Serial settings: `115200 8N1`.

串口参数：`115200 8N1`。

## Board A SENSOR Wiring / 板 A SENSOR 接线

| Module / 模块 | Module pin / 模块引脚 | STM32 pin / STM32 引脚 | Note / 说明 |
|---|---|---|---|
| DHT11 | DATA | `PB12` | One-wire data with pull-up / 单总线数据，需上拉 |
| DHT11 | VCC/GND | `3V3/GND` | 3.3 V reference supply / 参考供电 3.3 V |
| MQ135 | AO | `PA4 / ADC1_CH4` | Raw ADC; keep <= 3.3 V / 原始 ADC，需 <= 3.3 V |
| MQ135 | VCC/GND | Module-rated supply/GND | Heater modules often need 5 V / 加热型模块常需 5 V |
| MQ2 | AO | `PA5 / ADC1_CH5` | Raw ADC; keep <= 3.3 V / 原始 ADC，需 <= 3.3 V |
| MQ2 | VCC/GND | Module-rated supply/GND | Share ground with MCU / 与 MCU 共地 |
| Rain sensor / 雨量模块 | SIG/AO | `PA6 / ADC1_CH6` | Wet threshold starts at raw 1400 / 湿态阈值先从 1400 起步 |
| Rain sensor / 雨量模块 | VCC/GND | `3V3/GND` | Keep SIG inside ADC range / SIG 不得超 ADC 范围 |
| Thermistor / 热敏模块 | AO | `PA7 / ADC1_CH7` | 10K NTC B3950 lookup / 10K NTC B3950 查表 |
| Thermistor / 热敏模块 | DO | `PB9` | Pull-up input, active-low hot / 上拉输入，低电平高温 |
| Thermistor / 热敏模块 | VCC/GND | `3V3/GND` | Reference supply 3.3 V / 参考供电 3.3 V |
| Flame sensor / 火焰模块 | DO | `PB13` | Active-low trigger / 低电平触发 |
| Flame sensor / 火焰模块 | VCC/GND | Module-rated supply/GND | DO must be STM32-safe / DO 电平需兼容 STM32 |

## Board B MONITOR Wiring / 板 B MONITOR 接线

| Module / 模块 | Module pin / 模块引脚 | STM32 pin / STM32 引脚 | Note / 说明 |
|---|---|---|---|
| SSD1306 OLED | SCL | `PB6` | Software I2C clock / 软件 I2C 时钟 |
| SSD1306 OLED | SDA | `PB7` | Software I2C data / 软件 I2C 数据 |
| SSD1306 OLED | VCC/GND | `3V3/GND` | Check module address `0x3C` or `0x3D` / 检查地址 `0x3C` 或 `0x3D` |
| Active buzzer / 有源蜂鸣器 | SIG | `PB8` | Active-high / 高电平响 |
| Active buzzer / 有源蜂鸣器 | VCC/GND | `3V3/GND` | Confirm module voltage rating / 确认模块额定电压 |
| WS2813E RGB | DIN | `PA6 / TIM3_CH1` | GRB order, 800 kHz PWM + DMA / GRB 顺序，800 kHz PWM + DMA |
| WS2813E RGB | VCC/GND | `5V/GND` | Common ground; level shifter recommended / 共地；建议电平转换 |

## Optional W25Q64 / 可选 W25Q64

Connect W25Q64 only to Board B. The system still monitors and alarms without it. Firmware stores metadata in sector 0 and fixed 32-byte circular records from sector 1 through the end of the 8 MB chip.

W25Q64 只接板 B。不接时系统仍可监测和报警。固件把 sector 0 用作元数据区，从 sector 1 到 8 MB 末尾写固定 32 字节环形记录。

| W25Q64 pin / 引脚 | STM32 pin / STM32 引脚 | Note / 说明 |
|---|---|---|
| `CS` | `PB12` | Chip select / 片选 |
| `SCK/CLK` | `PB13` | SPI2_SCK |
| `SO/DO/MISO` | `PB14` | SPI2_MISO |
| `SI/DI/MOSI` | `PB15` | SPI2_MOSI |
| `VCC` | `3V3` | Never 5 V / 严禁 5 V |
| `GND` | `GND` | Common ground / 共地 |

## Power-On Checks / 上电检查

1. Check common GND between both boards. / 检查两块板是否共地。
2. Check Board A `PB10` to Board B `PB11`. / 检查板 A `PB10` 是否接到板 B `PB11`。
3. Check Board B `PB10` to Board A `PB11`. / 检查板 B `PB10` 是否接到板 A `PB11`。
4. Open debug serial at `115200 8N1`. / 以 `115200 8N1` 打开调试串口。
5. Board A should print `[SENSOR]`; Board B should print `[MONITOR] rx v2` and JSON. / 板 A 应输出 `[SENSOR]`，板 B 应输出 `[MONITOR] rx v2` 和 JSON。
6. Verify rain, thermistor, W25Q64, and WS2813E one at a time before full integration. / 全量联调前，逐个验证雨量、热敏、W25Q64 和 WS2813E。
