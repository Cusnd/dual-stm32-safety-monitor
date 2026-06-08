# Wiring Guide / 接线说明

[README](README.md) / [中文 README](README.zh-CN.md) | [Hardware reference](docs/hardware/index.en.md) / [硬件资料](docs/hardware/index.zh-CN.md)

This bilingual guide describes the current reference pin map for the active firmware. If you move to another STM32F103C8T6 board, keep the signal roles and electrical limits, or update firmware and docs together.

本文档以中英双语说明当前现役固件的参考引脚映射。若迁移到其他 STM32F103C8T6 板卡，请保持信号角色和电气限制一致；若改脚，需要同步修改固件和文档。

## Board-Porting Checklist / 板卡移植检查表

| Check / 检查项 | Requirement / 要求 |
|---|---|
| MCU supply / MCU 供电 | 3.3 V logic, stable VDDA for ADC / 3.3 V 逻辑电平，VDDA 稳定 |
| ADC inputs / ADC 输入 | Never exceed VDDA; add dividers/clamps for 5 V modules / 不得超过 VDDA；5 V 模块需分压或限幅 |
| Debug UART / 调试串口 | USART1 `PA9/PA10`, `115200 8N1`; MONITOR emits JSON Lines here / USART1 `PA9/PA10`，`115200 8N1`；MONITOR 从这里输出 JSON Lines |
| Board link / 双板通信 | USART3 `PB10/PB11`, crossed TX/RX, common GND / USART3 `PB10/PB11`，TX/RX 交叉，共地 |
| SWD / 下载调试 | Keep `PA13/PA14` free / 保留 `PA13/PA14` |
| Optional flash / 可选 Flash | W25Q64 is 3.3 V only; never drive it with 5 V signals / W25Q64 只能使用 3.3 V，禁止 5 V 供电或信号 |

## Reserved Or Board-Dependent Pins / 保留或板卡相关引脚

| Pin / 引脚 | Current reference use / 当前参考用途 | Porting note / 移植说明 |
|---|---|---|
| `PA9` | USART1_TX debug/JSON / USART1_TX 调试与 JSON | Connect to USB-UART RX if available / 有 USB 转串口时接其 RX |
| `PA10` | USART1_RX debug / USART1_RX 调试 | Connect to USB-UART TX if available / 有 USB 转串口时接其 TX |
| `PA11/PA12` | USB DM/DP reserved / USB DM/DP 保留 | Do not reuse when USB is populated / 接了 USB 时不复用 |
| `PA13/PA14` | SWD / SWD 下载调试 | Keep free / 保留 |
| `PA0/PC13` | K1/K2 on MONITOR / MONITOR 的 K1/K2 | Reference board uses external pull-down and active-high press / 参考板外部下拉，按下为高电平 |
| `PA1/PA2/PA3` | Not used by current app sources / 当前应用源码未使用 | Older demos used board RGB here; do not document it as active output unless you restore firmware support / 旧演示曾用于板载 RGB；除非恢复固件支持，否则不要作为当前输出说明 |
| `PA6` | SENSOR rain ADC only / SENSOR 雨量 ADC | Current MONITOR image does not drive WS2813 on this pin / 当前 MONITOR 镜像不再用该脚驱动 WS2813 |

## Board-to-Board USART3 Link / 双板 USART3 连接

| Board A SENSOR / 板 A | Board B MONITOR / 板 B | Note / 说明 |
|---|---|---|
| `PB10 / USART3_TX` | `PB11 / USART3_RX` | Sensor frames to monitor / 采集帧发往显示节点 |
| `PB11 / USART3_RX` | `PB10 / USART3_TX` | Reserved reverse path / 预留反向通道 |
| `GND` | `GND` | Required common ground / 必须共地 |

Serial settings: `115200 8N1`. The active protocol is a 22-byte v2 binary frame decoded by `FrameStreamDecoder`.

串口参数：`115200 8N1`。当前协议是 22 字节 v2 二进制帧，由 `FrameStreamDecoder` 流式解码。

## Board A SENSOR Wiring / 板 A SENSOR 接线

| Module / 模块 | Module pin / 模块引脚 | STM32 pin / STM32 引脚 | Note / 说明 |
|---|---|---|---|
| DHT11 | DATA | `PB12` | One-wire data with pull-up / 单总线数据，需上拉 |
| DHT11 | VCC/GND | `3V3/GND` | 3.3 V reference supply / 参考供电 3.3 V |
| MQ135 | AO | `PA4 / ADC1_CH4` | Raw ADC; keep <= VDDA / 原始 ADC，需 <= VDDA |
| MQ135 | VCC/GND | Module-rated supply/GND | Heater modules often need 5 V; share GND / 加热型模块常需 5 V；必须共地 |
| MQ2 | AO | `PA5 / ADC1_CH5` | Raw ADC; keep <= VDDA / 原始 ADC，需 <= VDDA |
| MQ2 | VCC/GND | Module-rated supply/GND | Share ground with MCU / 与 MCU 共地 |
| Rain sensor / 雨量模块 | AO/SIG | `PA6 / ADC1_CH6` | Default wet threshold is raw `1400`; direction may vary by module / 默认湿态阈值为原始值 `1400`；不同模块方向可能不同 |
| Rain sensor / 雨量模块 | VCC/GND | `3V3/GND` preferred | Keep AO/SIG inside ADC range / AO/SIG 不得超过 ADC 范围 |
| Thermistor / 热敏模块 | AO | `PA7 / ADC1_CH7` | 10K NTC B3950 lookup outputs `thermC10` in 0.1 deg C / 10K NTC B3950 查表，输出 0.1 摄氏度单位的 `thermC10` |
| Thermistor / 热敏模块 | DO | `PB9` | Pull-up input, active-low high-temperature trigger / 上拉输入，低电平高温触发 |
| Thermistor / 热敏模块 | VCC/GND | `3V3/GND` preferred | Reference supply 3.3 V / 参考供电 3.3 V |
| Flame sensor / 火焰模块 | DO | `PB13` | Active-low trigger / 低电平触发 |
| Flame sensor / 火焰模块 | VCC/GND | Module-rated supply/GND | DO must be STM32-safe / DO 电平需兼容 STM32 |

## Board B MONITOR Wiring / 板 B MONITOR 接线

| Module / 模块 | Module pin / 模块引脚 | STM32 pin / STM32 引脚 | Note / 说明 |
|---|---|---|---|
| SSD1306 OLED | SCL | `PB6` | Software I2C clock / 软件 I2C 时钟 |
| SSD1306 OLED | SDA | `PB7` | Software I2C data / 软件 I2C 数据 |
| SSD1306 OLED | VCC/GND | `3V3/GND` | Driver uses address `0x3C`; check module solder jumper if blank / 驱动使用地址 `0x3C`；若无显示请检查模块焊盘地址 |
| Active buzzer / 有源蜂鸣器 | SIG | `PB8` | Active-high; danger beeps fast, node-lost beeps slowly unless muted / 高电平响；危险快速鸣叫，节点离线慢速提示，静音时关闭 |
| Active buzzer / 有源蜂鸣器 | VCC/GND | `3V3/GND` | Confirm module voltage rating / 确认模块额定电压 |
| K1 | on-board key | `PA0` | Press to switch OLED page / 按下切换 OLED 页面 |
| K2 | on-board key | `PC13` | Short press mutes buzzer for 60 s; long press changes threshold profile / 短按蜂鸣器静音 60 秒；长按切换阈值档位 |

## Optional W25Q64 / 可选 W25Q64

Connect W25Q64 only to Board B. The system still monitors, displays, alarms, and streams JSON without it. Firmware stores cursor metadata in sector 0 and fixed 32-byte circular records from sector 1 through the end of the 8 MB chip.

W25Q64 只接板 B。不接时系统仍可监测、显示、报警并输出 JSON。固件把 sector 0 用作游标元数据区，从 sector 1 到 8 MB 末尾写固定 32 字节环形记录。

| W25Q64 pin / 引脚 | STM32 pin / STM32 引脚 | Note / 说明 |
|---|---|---|
| `CS` | `PB12` | GPIO chip select / GPIO 片选 |
| `SCK/CLK` | `PB13` | SPI2_SCK |
| `SO/DO/MISO` | `PB14` | SPI2_MISO |
| `SI/DI/MOSI` | `PB15` | SPI2_MOSI |
| `VCC` | `3V3` | Never 5 V / 严禁 5 V |
| `GND` | `GND` | Common ground / 共地 |

## Power-On Checks / 上电检查

1. Check common GND between both boards. / 检查两块板是否共地。
2. Check Board A `PB10` to Board B `PB11`. / 检查板 A `PB10` 是否接到板 B `PB11`。
3. Check Board B `PB10` to Board A `PB11` if the reverse link is wired. / 若连接反向链路，检查板 B `PB10` 是否接到板 A `PB11`。
4. Open Board B debug serial at `115200 8N1`. / 以 `115200 8N1` 打开板 B 调试串口。
5. Board A should print `[SENSOR]`; Board B should print `[MONITOR] rx v2` and JSON Lines. / 板 A 应输出 `[SENSOR]`；板 B 应输出 `[MONITOR] rx v2` 和 JSON Lines。
6. Verify rain, thermistor, buzzer, keys, OLED, and W25Q64 one at a time before full integration. / 全量联调前，逐个验证雨量、热敏、蜂鸣器、按键、OLED 和 W25Q64。
