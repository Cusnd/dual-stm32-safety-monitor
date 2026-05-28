# Wiring Guide

This guide lists the wiring needed to run the dual-board STM32 safety monitor.

## Reserved Board Pins

Do not reuse these for external modules:

| Pin | Board function | Project use |
|---|---|---|
| `PA9` | USART1_TX to CH340C | USB serial debug |
| `PA10` | USART1_RX to CH340C | USB serial debug |
| `PA11/PA12` | USB Device | Reserved |
| `PA13/PA14` | SWD | Programming/debug |
| `PA1/PA2/PA3` | On-board RGB LED | Monitor alarm LED |
| `PA0` | K1 | OLED page switch |
| `PC13` | K2 | Mute / threshold profile |

## Board-to-Board USART3 Link

TX and RX must be crossed, and both boards must share GND.

| Board A SENSOR | Board B MONITOR | Note |
|---|---|---|
| `PB10 / USART3_TX` | `PB11 / USART3_RX` | Sensor data to monitor |
| `PB11 / USART3_RX` | `PB10 / USART3_TX` | Reserved for future ACK/heartbeat |
| `GND` | `GND` | Required common reference |

Serial settings: `115200 8N1`.

## Board A SENSOR Wiring

| Module | Module pin | STM32 pin | Note |
|---|---|---|---|
| DHT11 | DATA | `PB12` | Single-wire data |
| DHT11 | VCC | `3V3` | Follow your module label |
| DHT11 | GND | `GND` | Common ground |
| MQ135 | AO | `PA4` | ADC1_CH4 |
| MQ135 | VCC | `5V` or module supply | Ensure AO does not exceed 3.3V |
| MQ135 | GND | `GND` | Common ground |
| MQ2 | AO | `PA5` | ADC1_CH5 |
| MQ2 | VCC | `5V` or module supply | Ensure AO does not exceed 3.3V |
| MQ2 | GND | `GND` | Common ground |
| Flame sensor | DO | `PB13` | Active-low trigger |
| Flame sensor | VCC | `3V3/5V` as required | Ensure logic output is 3.3V-compatible |
| Flame sensor | GND | `GND` | Common ground |

## Board B MONITOR Wiring

| Module | Module pin | STM32 pin | Note |
|---|---|---|---|
| OLED | SCL | `PB6` | Software I2C clock |
| OLED | SDA | `PB7` | Software I2C data |
| OLED | VCC | `3V3` | SSD1306 I2C OLED |
| OLED | GND | `GND` | Common ground |
| Active buzzer | SIG | `PB8` | Active-high |
| Active buzzer | VCC | `3V3/5V` as required | Active buzzer recommended |
| Active buzzer | GND | `GND` | Common ground |

## Optional W25Q64

Connect only to Board B. The system still works without this module.

| W25Q64 pin | STM32 pin | Note |
|---|---|---|
| `CS` | `PB12` | Chip select |
| `SCK` | `PB13` | SPI2_SCK |
| `SO / MISO` | `PB14` | SPI2_MISO |
| `SI / MOSI` | `PB15` | SPI2_MOSI |
| `VCC` | `3V3` | Do not use 5V |
| `GND` | `GND` | Common ground |

## Power-On Checks

1. Check common GND between both boards.
2. Check Board A `PB10` to Board B `PB11`.
3. Check Board B `PB10` to Board A `PB11`.
4. Leave `PA9/PA10` free for USB serial debug.
5. Open both serial ports at `115200 8N1`.
6. Board A should print `[SENSOR]` logs.
7. Board B should print `[MONITOR] rx` logs and refresh OLED.
