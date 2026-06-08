# Hardware Reference

[README](../../README.md) | [中文](index.zh-CN.md) | [Wiring](../../WIRING.md)

This directory documents the project as a generic STM32F103C8T6 dual-board reference design. The current firmware pin map is treated as the reference implementation, not as a vendor-specific board requirement.

## Chip References

| Topic | File |
|---|---|
| STM32F103C8T6 MCU | [chips/stm32f103c8t6.en.md](chips/stm32f103c8t6.en.md) |
| CH340C USB-UART | [chips/ch340c.en.md](chips/ch340c.en.md) |
| DHT11 sensor IC | [chips/dht11.en.md](chips/dht11.en.md) |
| MQ135 gas sensor element | [chips/mq135.en.md](chips/mq135.en.md) |
| MQ2 gas sensor element | [chips/mq2.en.md](chips/mq2.en.md) |
| LM393 comparator | [chips/lm393.en.md](chips/lm393.en.md) |
| SSD1306 OLED controller | [chips/ssd1306.en.md](chips/ssd1306.en.md) |
| W25Q64 SPI NOR flash | [chips/w25q64.en.md](chips/w25q64.en.md) |
| WS2813E addressable RGB LED (legacy/reference) | [chips/ws2813e.en.md](chips/ws2813e.en.md) |
| 10K NTC B3950 thermistor | [chips/10k-ntc-b3950.en.md](chips/10k-ntc-b3950.en.md) |

## Module References

| Topic | File |
|---|---|
| DHT11 module | [modules/dht11-module.en.md](modules/dht11-module.en.md) |
| MQ135 module | [modules/mq135-module.en.md](modules/mq135-module.en.md) |
| MQ2 module | [modules/mq2-module.en.md](modules/mq2-module.en.md) |
| Rain sensor module | [modules/rain-sensor-module.en.md](modules/rain-sensor-module.en.md) |
| Thermistor module | [modules/thermistor-module.en.md](modules/thermistor-module.en.md) |
| Flame sensor module | [modules/flame-sensor-module.en.md](modules/flame-sensor-module.en.md) |
| SSD1306 OLED module | [modules/oled-ssd1306-module.en.md](modules/oled-ssd1306-module.en.md) |
| Active buzzer module | [modules/active-buzzer-module.en.md](modules/active-buzzer-module.en.md) |
| W25Q64 module | [modules/w25q64-module.en.md](modules/w25q64-module.en.md) |
| WS2813E RGB module (legacy/reference) | [modules/ws2813e-rgb-module.en.md](modules/ws2813e-rgb-module.en.md) |
| USART3 board link | [modules/usart3-board-link.en.md](modules/usart3-board-link.en.md) |

## Generic Board Porting Checklist

- Keep `PA9/PA10` available for USART1 debug only if the chosen board routes them to a USB-UART bridge; otherwise map the debug port explicitly.
- Verify all ADC inputs stay within 0 to VDDA. The reference design assumes 3.3 V ADC full scale.
- Verify the board exposes `PB10/PB11` for the board-to-board USART3 link, or update firmware and docs together.
- Keep SWD pins `PA13/PA14` free during development.
- Treat all module schematic snippets as generic reference circuits. Module vendors may change resistor values, pull-ups, comparators, or connector order.
