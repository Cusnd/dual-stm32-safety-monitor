# 模块参考

[中文 README](../README.zh-CN.md) | [English](MODULE_REFERENCE.en.md) | [硬件索引](hardware/index.zh-CN.md) | [接线说明](../WIRING.md)

本文档现在作为模块级索引。每个模块的详细说明放在 `docs/hardware/modules/`。

## 参考模块映射

| 模块 | 节点 | 参考信号 | 详细文档 |
|---|---|---|---|
| DHT11 | SENSOR | `DATA -> PB12` | [DHT11 模块](hardware/modules/dht11-module.zh-CN.md) |
| MQ135 | SENSOR | `AO -> PA4/ADC1_CH4` | [MQ135 模块](hardware/modules/mq135-module.zh-CN.md) |
| MQ2 | SENSOR | `AO -> PA5/ADC1_CH5` | [MQ2 模块](hardware/modules/mq2-module.zh-CN.md) |
| 雨量模块 | SENSOR | `SIG -> PA6/ADC1_CH6` | [雨量模块](hardware/modules/rain-sensor-module.zh-CN.md) |
| 热敏模块 | SENSOR | `AO -> PA7/ADC1_CH7`，`DO -> PB9` | [热敏模块](hardware/modules/thermistor-module.zh-CN.md) |
| 火焰模块 | SENSOR | `DO -> PB13` | [火焰模块](hardware/modules/flame-sensor-module.zh-CN.md) |
| SSD1306 OLED | MONITOR | `SCL/SDA -> PB6/PB7` | [OLED 模块](hardware/modules/oled-ssd1306-module.zh-CN.md) |
| 有源蜂鸣器 | MONITOR | `SIG -> PB8` | [有源蜂鸣器模块](hardware/modules/active-buzzer-module.zh-CN.md) |
| W25Q64 | MONITOR | `CS/SCK/MISO/MOSI -> PB12/PB13/PB14/PB15` | [W25Q64 模块](hardware/modules/w25q64-module.zh-CN.md) |
| WS2813E RGB | MONITOR | `DIN -> PA6/TIM3_CH1` | [WS2813E RGB 模块](hardware/modules/ws2813e-rgb-module.zh-CN.md) |
| USART3 链路 | 两块板 | `PB10/PB11` 交叉 | [USART3 双板链路](hardware/modules/usart3-board-link.zh-CN.md) |

## 信号规则

- 模拟模块输出必须确认落在 STM32 0..VDDA 范围。
- 数字模块输出必须确认极性和 3.3 V 兼容性。
- 模块原理图会因厂商不同而变化；单模块文档描述通用参考电路和本项目映射。
