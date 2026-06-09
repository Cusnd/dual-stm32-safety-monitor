# Board And Chip Reference

[README](../README.md) | [Chinese](BOARD_AND_CHIP_REFERENCE.zh-CN.md) | [Hardware index](hardware/index.en.md) | [Wiring](../WIRING.md)

This document is now a board-level index for the generic STM32F103C8T6 dual-board reference design. Detailed chip notes live under `docs/hardware/chips/`.

## Board-Level Summary

| Item | Reference design choice | Porting rule |
|---|---|---|
| MCU | STM32F103C8T6 LQFP48 | Keep equivalent peripherals or update firmware mapping |
| Clock | 8 MHz HSE to 72 MHz SYSCLK | Recalculate UART/SPI/timer timing if clock changes |
| Debug UART | USART1 `PA9/PA10` | Target board may use any USB-UART bridge if mapped correctly |
| Board link | USART3 `PB10/PB11` | Cross TX/RX and share GND |
| Analog inputs | `PA4/PA5/PA6/PA7` | Keep input voltage inside 0..VDDA |
| Display | SSD1306 over bit-banged `PB6/PB7` | I2C address and pull-ups are board/module dependent |
| Optional log | W25Q64 over SPI2 | 3.3 V only; monitor still works without it |
| Legacy expansion | WS2813E hardware notes remain available | Current CMake build does not compile a WS2813 driver |

## Chip Documents

| Chip/topic | Document |
|---|---|
| STM32F103C8T6 MCU | [hardware/chips/stm32f103c8t6.en.md](hardware/chips/stm32f103c8t6.en.md) |
| CH340C USB-UART bridge | [hardware/chips/ch340c.en.md](hardware/chips/ch340c.en.md) |
| DHT11 | [hardware/chips/dht11.en.md](hardware/chips/dht11.en.md) |
| MQ135 | [hardware/chips/mq135.en.md](hardware/chips/mq135.en.md) |
| MQ2 | [hardware/chips/mq2.en.md](hardware/chips/mq2.en.md) |
| LM393 comparator | [hardware/chips/lm393.en.md](hardware/chips/lm393.en.md) |
| SSD1306 OLED controller | [hardware/chips/ssd1306.en.md](hardware/chips/ssd1306.en.md) |
| W25Q64 SPI NOR flash | [hardware/chips/w25q64.en.md](hardware/chips/w25q64.en.md) |
| WS2813E RGB LED (legacy/reference) | [hardware/chips/ws2813e.en.md](hardware/chips/ws2813e.en.md) |
| 10K NTC B3950 | [hardware/chips/10k-ntc-b3950.en.md](hardware/chips/10k-ntc-b3950.en.md) |

## Project Artifact Names

`Env-Monitor.ioc`, `Env-Monitor_sensor.hex`, and `Env-Monitor_monitor.hex` are project artifact names. They are not board-vendor requirements.
