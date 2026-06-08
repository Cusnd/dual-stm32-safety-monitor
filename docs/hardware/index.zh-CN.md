# 硬件参考资料

[中文 README](../../README.zh-CN.md) | [English](index.en.md) | [接线说明](../../WIRING.md)

本目录把项目整理为通用 STM32F103C8T6 双板参考设计。当前固件引脚表是“参考实现引脚映射”，不是某个厂商开发板的专属接法。

## 芯片资料

| 主题 | 文件 |
|---|---|
| STM32F103C8T6 MCU | [chips/stm32f103c8t6.zh-CN.md](chips/stm32f103c8t6.zh-CN.md) |
| CH340C USB 转串口 | [chips/ch340c.zh-CN.md](chips/ch340c.zh-CN.md) |
| DHT11 传感芯片 | [chips/dht11.zh-CN.md](chips/dht11.zh-CN.md) |
| MQ135 气敏元件 | [chips/mq135.zh-CN.md](chips/mq135.zh-CN.md) |
| MQ2 气敏元件 | [chips/mq2.zh-CN.md](chips/mq2.zh-CN.md) |
| LM393 比较器 | [chips/lm393.zh-CN.md](chips/lm393.zh-CN.md) |
| SSD1306 OLED 控制器 | [chips/ssd1306.zh-CN.md](chips/ssd1306.zh-CN.md) |
| W25Q64 SPI NOR Flash | [chips/w25q64.zh-CN.md](chips/w25q64.zh-CN.md) |
| WS2813E 可寻址 RGB LED（legacy/reference） | [chips/ws2813e.zh-CN.md](chips/ws2813e.zh-CN.md) |
| 10K NTC B3950 热敏电阻 | [chips/10k-ntc-b3950.zh-CN.md](chips/10k-ntc-b3950.zh-CN.md) |

## 模块资料

| 主题 | 文件 |
|---|---|
| DHT11 模块 | [modules/dht11-module.zh-CN.md](modules/dht11-module.zh-CN.md) |
| MQ135 模块 | [modules/mq135-module.zh-CN.md](modules/mq135-module.zh-CN.md) |
| MQ2 模块 | [modules/mq2-module.zh-CN.md](modules/mq2-module.zh-CN.md) |
| 雨量模块 | [modules/rain-sensor-module.zh-CN.md](modules/rain-sensor-module.zh-CN.md) |
| 热敏模块 | [modules/thermistor-module.zh-CN.md](modules/thermistor-module.zh-CN.md) |
| 火焰模块 | [modules/flame-sensor-module.zh-CN.md](modules/flame-sensor-module.zh-CN.md) |
| SSD1306 OLED 模块 | [modules/oled-ssd1306-module.zh-CN.md](modules/oled-ssd1306-module.zh-CN.md) |
| 有源蜂鸣器模块 | [modules/active-buzzer-module.zh-CN.md](modules/active-buzzer-module.zh-CN.md) |
| W25Q64 模块 | [modules/w25q64-module.zh-CN.md](modules/w25q64-module.zh-CN.md) |
| WS2813E RGB 模块（legacy/reference） | [modules/ws2813e-rgb-module.zh-CN.md](modules/ws2813e-rgb-module.zh-CN.md) |
| USART3 双板链路 | [modules/usart3-board-link.zh-CN.md](modules/usart3-board-link.zh-CN.md) |

## 通用板卡移植检查表

- 只有当目标板把 `PA9/PA10` 接到 USB 转串口时，才能直接沿用 USART1 调试口；否则需要明确改映射。
- 所有 ADC 输入必须落在 0 到 VDDA 之间；参考实现按 3.3 V ADC 满量程设计。
- 确认目标板能引出 `PB10/PB11` 做 USART3 双板通信；如果改脚，固件和文档必须同步。
- 开发阶段保留 SWD `PA13/PA14`。
- 本目录中的原理图都是通用参考电路。不同模块厂商可能调整电阻、上拉、比较器或接口顺序，最终以实物丝印和原理图为准。
