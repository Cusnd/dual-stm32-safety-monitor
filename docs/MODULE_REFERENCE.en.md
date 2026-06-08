# Module Reference

[README](../README.md) | [中文](MODULE_REFERENCE.zh-CN.md) | [Hardware index](hardware/index.en.md) | [Wiring](../WIRING.md)

This document is now the module-level index. Detailed per-module notes live under `docs/hardware/modules/`.

## Reference Module Map

| Module | Node | Reference signal | Detailed document |
|---|---|---|---|
| DHT11 | SENSOR | `DATA -> PB12` | [DHT11 module](hardware/modules/dht11-module.en.md) |
| MQ135 | SENSOR | `AO -> PA4/ADC1_CH4` | [MQ135 module](hardware/modules/mq135-module.en.md) |
| MQ2 | SENSOR | `AO -> PA5/ADC1_CH5` | [MQ2 module](hardware/modules/mq2-module.en.md) |
| Rain sensor | SENSOR | `SIG -> PA6/ADC1_CH6` | [Rain sensor module](hardware/modules/rain-sensor-module.en.md) |
| Thermistor | SENSOR | `AO -> PA7/ADC1_CH7`, `DO -> PB9` | [Thermistor module](hardware/modules/thermistor-module.en.md) |
| Flame sensor | SENSOR | `DO -> PB13` | [Flame sensor module](hardware/modules/flame-sensor-module.en.md) |
| SSD1306 OLED | MONITOR | `SCL/SDA -> PB6/PB7` | [OLED module](hardware/modules/oled-ssd1306-module.en.md) |
| Active buzzer | MONITOR | `SIG -> PB8` | [Active buzzer module](hardware/modules/active-buzzer-module.en.md) |
| W25Q64 | MONITOR | `CS/SCK/MISO/MOSI -> PB12/PB13/PB14/PB15` | [W25Q64 module](hardware/modules/w25q64-module.en.md) |
| WS2813E RGB | Legacy expansion | `DIN -> PA6/TIM3_CH1` in old demos | [WS2813E RGB module](hardware/modules/ws2813e-rgb-module.en.md) |
| USART3 link | Both | `PB10/PB11` crossed | [USART3 board link](hardware/modules/usart3-board-link.en.md) |

## Signal Policy

- ADC module outputs must be verified against the STM32 0..VDDA range.
- Digital module outputs must be checked for polarity and 3.3 V compatibility.
- Module schematics vary by vendor; the per-module pages describe generic reference circuits and project mapping.
- The current MONITOR firmware does not compile a WS2813/RGB output path; keep those pages as reference unless support is restored.
