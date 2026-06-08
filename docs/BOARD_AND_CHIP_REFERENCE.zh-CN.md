# 开发板与芯片参考

[中文 README](../README.zh-CN.md) | [English](BOARD_AND_CHIP_REFERENCE.en.md) | [硬件索引](hardware/index.zh-CN.md) | [接线说明](../WIRING.md)

本文档现在作为通用 STM32F103C8T6 双板参考设计的板级索引。详细芯片说明放在 `docs/hardware/chips/`。

## 板级总览

| 项目 | 参考设计选择 | 移植规则 |
|---|---|---|
| MCU | STM32F103C8T6 LQFP48 | 保持等价外设，或同步更新固件映射 |
| 时钟 | 8 MHz HSE 到 72 MHz SYSCLK | 若时钟变化，重新计算 UART/SPI/TIM 时序 |
| 调试串口 | USART1 `PA9/PA10` | 目标板可用任意 USB 转串口，只要映射正确 |
| 双板链路 | USART3 `PB10/PB11` | TX/RX 交叉并共地 |
| 模拟输入 | `PA4/PA5/PA6/PA7` | 输入电压必须在 0..VDDA |
| 显示 | SSD1306，软件 I2C `PB6/PB7` | I2C 地址和上拉取决于模块 |
| 外置日志 | W25Q64，SPI2 | 只能 3.3 V 供电 |
| 外置 RGB | MONITOR 固件使用 `PA6/TIM3_CH1` 驱动 WS2813E | 灯珠 5 V 供电，共地，建议电平转换 |

## 芯片文档

| 芯片/主题 | 文档 |
|---|---|
| STM32F103C8T6 MCU | [hardware/chips/stm32f103c8t6.zh-CN.md](hardware/chips/stm32f103c8t6.zh-CN.md) |
| CH340C USB 转串口 | [hardware/chips/ch340c.zh-CN.md](hardware/chips/ch340c.zh-CN.md) |
| DHT11 | [hardware/chips/dht11.zh-CN.md](hardware/chips/dht11.zh-CN.md) |
| MQ135 | [hardware/chips/mq135.zh-CN.md](hardware/chips/mq135.zh-CN.md) |
| MQ2 | [hardware/chips/mq2.zh-CN.md](hardware/chips/mq2.zh-CN.md) |
| LM393 比较器 | [hardware/chips/lm393.zh-CN.md](hardware/chips/lm393.zh-CN.md) |
| SSD1306 OLED 控制器 | [hardware/chips/ssd1306.zh-CN.md](hardware/chips/ssd1306.zh-CN.md) |
| W25Q64 SPI NOR Flash | [hardware/chips/w25q64.zh-CN.md](hardware/chips/w25q64.zh-CN.md) |
| WS2813E RGB LED | [hardware/chips/ws2813e.zh-CN.md](hardware/chips/ws2813e.zh-CN.md) |
| 10K NTC B3950 | [hardware/chips/10k-ntc-b3950.zh-CN.md](hardware/chips/10k-ntc-b3950.zh-CN.md) |

## 项目产物命名

`Env-Monitor.ioc`、`Env-Monitor_sensor.hex`、`Env-Monitor_monitor.hex` 是当前项目产物名，不代表必须使用某个厂商硬件。
