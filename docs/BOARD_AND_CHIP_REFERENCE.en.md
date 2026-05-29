# Board And Chip Reference

[English README](../README.md) | [中文](BOARD_AND_CHIP_REFERENCE.zh-CN.md) | [Wiring Guide](../WIRING.md) | [Module Reference](MODULE_REFERENCE.en.md) | [CLion + CMake](CLION_CMAKE_GUIDE.en.md)

This document describes the chip, core board, and expansion board used by the project. It answers:

1. Which chip and boards the project uses.
2. Why the board-level circuits and chip peripherals support the current design.
3. How important pins are assigned, and which pins should not be reused casually.

Reference material includes the Fire STM32F103C8T6 dual-USB core-board manual, schematics, pin-assignment sheet, Fire STM32F103C8T6 expansion-board manual and schematics, the ST STM32F103x8 datasheet, and the STM32F10x reference manual.

## 1. Overview Checklist

| Category | Item | Project Role | Key Points |
|---|---|---|---|
| MCU | STM32F103C8T6 | Used on both boards | Cortex-M3, 72 MHz, 64 KB Flash, 20 KB SRAM, LQFP48 |
| Core board | Fire STM32F103C8T6 dual-USB core board | Board A acquisition node, Board B monitor/alarm node | On-board CH340C, SWD, RGB LED, K1/K2, 8 MHz HSE |
| Expansion board | Fire STM32F103C8T6 expansion board | Optional helper for direct-plug or jumper wiring | OLED can plug directly; several modules must follow project jumper wiring |
| Debug/download | CH340C USB-UART, SWD | Serial logs, ISP download, SWD debug | Keep `PA9/PA10` for USART1 debug and `PA13/PA14` for SWD |
| Board-to-board link | USART3 | Board A sends sensor frames to Board B | Cross `PB10/PB11`, 115200 8N1 |

## 2. STM32F103C8T6 Chip

### 2.1 Chip Checklist

| Item | Description | Project Usage |
|---|---|---|
| Core | ARM Cortex-M3 | Main loops, USART interrupt, DWT microsecond delay |
| Clock | Up to 72 MHz | 8 MHz HSE multiplied by PLL to 72 MHz |
| Flash | 64 KB | Both images fit comfortably |
| SRAM | 20 KB | Both images use about 10% RAM |
| Supply | Digital supply 2.0-3.6 V; VDDA at least 2.4 V when ADC is used | Core board provides 3.3 V |
| ADC | 12-bit ADC with multiple channels | `PA4/ADC1_CH4` for MQ135, `PA5/ADC1_CH5` for MQ2 |
| USART | USART1, USART2, USART3 | USART1 for USB debug logs, USART3 for board-to-board link |
| SPI | SPI1, SPI2 | Optional W25Q64 uses SPI2 |
| GPIO | Input, output, alternate function, analog input | Sensors, OLED, buzzer, RGB LED, buttons |
| Debug | SWD/JTAG | Project reserves `PA13/PA14` for SWD |

### 2.2 Clock Design

The core board provides an 8 MHz external high-speed oscillator. `SystemClock_Config()` configures:

| Clock | Configuration | Purpose |
|---|---|---|
| HSE | 8 MHz | PLL input |
| PLL | 8 MHz x 9 | Generates 72 MHz SYSCLK |
| SYSCLK | 72 MHz | Cortex-M3 core and most peripheral timing |
| APB1 | 36 MHz | USART3 and SPI2 bus |
| APB2 | 72 MHz | GPIO, ADC1, USART1 bus |
| ADC clock | APB2 / 6 = 12 MHz | Below the STM32F1 ADC 14 MHz limit |

This matches the chip datasheet, the board 8 MHz crystal, and the project serial/ADC timing.

### 2.3 Pin Capability Notes

| Pin | Chip Function | Project Use | Note |
|---|---|---|---|
| `PA4` | GPIO / ADC1_CH4 / SPI1_NSS | MQ135 AO | Analog input; never exceed 3.3 V |
| `PA5` | GPIO / ADC1_CH5 / SPI1_SCK | MQ2 AO | Analog input; never exceed 3.3 V |
| `PA9` | USART1_TX | USB serial debug output | Connected to CH340C through board jumpers |
| `PA10` | USART1_RX | USB serial debug input | Connected to CH340C through board jumpers |
| `PA11/PA12` | USB DM/DP | Reserved | Used by the core-board USB device port |
| `PA13/PA14` | SWDIO/SWCLK | Flashing/debug | Do not reuse for external modules |
| `PB6/PB7` | GPIO / I2C-related AF | OLED software I2C | Used as open-drain GPIO for bit-banged I2C |
| `PB8` | GPIO / TIM4_CH3 / CAN_RX | Buzzer SIG | Push-pull output, active high |
| `PB10/PB11` | USART3_TX/RX / I2C2 | USART3 board link | TX/RX must be crossed between boards |
| `PB12` | GPIO / SPI2_NSS / USART3_CK | Board A DHT11; Board B W25Q64 CS | Role-specific reuse; do not mix on one board |
| `PB13` | GPIO / SPI2_SCK / USART3_CTS | Board A flame DO; Board B W25Q64 SCK | Digital 5V-tolerant IO, not an ADC pin |
| `PB14` | GPIO / SPI2_MISO / USART3_RTS | W25Q64 MISO | Optional flash on monitor node |
| `PB15` | GPIO / SPI2_MOSI | W25Q64 MOSI | Do not press the expansion-board PB15 key during flash use |
| `PA0` | WKUP / GPIO / ADC | On-board K1 | High means pressed |
| `PC13` | GPIO | On-board K2 | High means pressed |
| `PA1/PA2/PA3` | GPIO / ADC / TIM2 | On-board RGB LED | Active low |

## 3. Core Board

### 3.1 Core-Board Checklist

| Module | Board Circuit | Project Role |
|---|---|---|
| MCU | STM32F103C8T6 LQFP48 | Main controller for both nodes |
| Power | USB 5 V input, LDO to 3.3 V | Powers MCU and 3.3 V modules |
| HSE | 8 MHz crystal | 72 MHz system clock source |
| USB-UART | CH340C/CH340X connected to `PA9/PA10` | USART1 debug logs and serial download |
| SWD | `NRST`, `PA13/SWDIO`, `PA14/SWCLK`, 3V3, GND | ST-Link flashing/debug |
| BOOT | BOOT0/BOOT1 pulled down by default | Boot from internal Flash |
| RGB LED | `PA1/PA2/PA3`, active low | Monitor-node state indicator |
| Buttons | K1=`PA0`, K2=`PC13`, active high | OLED page, mute, threshold profile |
| Headers | Exposes most MCU IO | Sensors, OLED, buzzer, W25Q64, USART3 link |

### 3.2 Core-Board Circuit Notes

The core board accepts USB power or external 5 V, then uses a 3.3 V LDO for the MCU and 3.3 V peripherals. All MCU IO logic is 3.3 V based:

- DHT11, OLED, and W25Q64 should use 3.3 V.
- MQ135/MQ2 can use 5 V for their heater/module supply, but AO into the STM32 ADC must stay at or below 3.3 V.
- If the flame module is powered from 5 V, confirm its DO output is safe for STM32 digital input.

USART1 is connected to CH340C through `PA9/PA10` and is reserved for debug logs. The project intentionally avoids assigning these pins to external modules.

The RGB LED anodes are tied to 3.3 V through LEDs/resistors, then to MCU pins. Driving the pin low turns the LED on. `LED_Set()` follows this active-low hardware polarity.

K1/K2 are active high and include pull-down/debounce components on the board. The firmware reads them as active-high buttons.

### 3.3 Reserved Core-Board Pins

| Pin | Board Connection | Project Handling |
|---|---|---|
| `PA9/PA10` | CH340C USB-UART | Reserved for USART1 debug logs |
| `PA11/PA12` | USB Device DM/DP | Reserved |
| `PA13/PA14` | SWDIO/SWCLK | Reserved for flashing/debug |
| `PD0/PD1` | 8 MHz HSE | Not used as GPIO |
| `PC14/PC15` | 32.768 kHz crystal footprint | Not used |
| `PA1/PA2/PA3` | On-board RGB LED | Monitor alarm LED |
| `PA0/PC13` | On-board K1/K2 | Monitor buttons |

## 4. Expansion Board

### 4.1 Expansion-Board Checklist

The expansion board is not required to run the project, but it helps organize wiring. It adapts core-board headers to common module connectors and provides 6-12 V DC input, 5 V/3.3 V rails, an OLED socket, general IO connectors, and communication connectors.

| Expansion Connector | Pins | Project Fit |
|---|---|---|
| OLED socket | `3V3/GND/PB6/PB7` | Board B OLED can plug directly |
| Communication 1/2 | `VCC/GND/PB11/PB10/PB8/PB9` | Can route USART3 and expose buzzer `PB8` |
| Communication 3 | `VCC/GND/PA10/PA9/PA12/PA11` | Not recommended for project modules; `PA9/PA10` are debug |
| Communication 4 | `VCC/GND/PB7/PB6/PA7/PA8` | Useful for OLED/I2C-like modules |
| General IO1 | `VCC/GND/PA12/PA5` | Can connect MQ2 AO to `PA5`; check orientation/voltage |
| General IO2 | `VCC/GND/PA11/PA4` | Can connect MQ135 AO to `PA4`; check orientation/voltage |
| General IO3 | `3V3/GND/NC/PB12` | Can connect Board A DHT11 |
| General IO4 | `3V3/GND/NC/PA6` | Not used by default |
| SPI1/W25Q64 socket | `VCC/GND/PA4/PA6/PA7/PA5` | Not suitable for this project's W25Q64, because firmware uses SPI2 |
| Expansion-board key | `PB15` | Do not press if Board B uses W25Q64 MOSI on PB15 |

### 4.2 Expansion-Board Power

The expansion board accepts 6-12 V DC, generates/distributes 5 V, and provides 3.3 V rails. Notes:

1. With only core-board USB power, expansion-board 3.3 V is available; expansion-board 5 V depends on the power switch state.
2. If MQ modules use expansion-board 5 V, press the expansion-board self-locking power switch.
3. Do not power 3.3 V modules from a 5 V selector position, especially OLED, DHT11, and W25Q64.
4. The two core boards must share GND even when powered by separate USB cables.

### 4.3 Expansion-Board Boundaries For This Project

Fire example projects often demonstrate one module at a time. This project is a multi-module, dual-board system, so several pins are intentionally changed:

| Module | Common Example / Direct-Plug Idea | Project Wiring | Reason |
|---|---|---|---|
| OLED | Some examples use `PB10/PB11`; expansion socket uses `PB6/PB7` | `PB6/PB7` | `PB10/PB11` are reserved for USART3 |
| W25Q64 | Expansion-board SPI1 `PA4/PA5/PA6/PA7` | SPI2 `PB12/PB13/PB14/PB15` | `PA4/PA5` are used by MQ ADC inputs |
| Flame sensor | Often `PA11` or `PA5` | `PB13` | `PA11/PA12` reserved for USB; `PA5` used by MQ2 ADC |
| DHT11 | Examples may use `PB0` or `PB12` | `PB12` | Good fit for one-wire; acquisition node does not use W25Q64 |
| Buzzer | Examples may use `PA6/PB9` | `PB8` | Dedicated monitor alarm output |
| MQ135/MQ2 | Both modules may default AO to `PA4` | MQ135=`PA4`, MQ2=`PA5` | Two analog modules need two ADC channels |

## 5. Board A / Board B Responsibilities

| Node | Firmware | Board Responsibility | Core-Board Resources |
|---|---|---|---|
| Board A SENSOR | `Fire_F103_sensor.hex` | Sample DHT11, MQ135, MQ2, and flame; send USART3 frames | ADC1, DWT, USART1, USART3, GPIOA/GPIOB |
| Board B MONITOR | `Fire_F103_monitor.hex` | Receive frames, update OLED, drive RGB/buzzer, handle K1/K2, optional flash logging | USART1, USART3, software I2C, SPI2, GPIO, SysTick |

Both boards use the same `Core/Src/main.c`. In CLion, selecting a CMake preset makes CMake pass `APP_NODE_ROLE`:

| Preset | Role Macro | Output |
|---|---|---|
| `SensorDebug` | `APP_NODE_ROLE=1` | `build/SensorDebug/Fire_F103_sensor.hex` |
| `MonitorDebug` | `APP_NODE_ROLE=2` | `build/MonitorDebug/Fire_F103_monitor.hex` |

The primary project entry is CLion + CMake Presets. `MDK-ARM/` is only retained as reference material.

## 6. Board-Level Power-On Checklist

| Check | Correct State | Failure Symptom |
|---|---|---|
| BOOT0/BOOT1 | Pulled down, boot from internal Flash | User program does not run, or board enters system bootloader |
| SWD | `PA13/PA14` free from external modules | Flashing/debug connection unstable |
| USART1 | `PA9/PA10` connected to CH340C | No serial log or serial download issue |
| USART3 | Cross `PB10` and `PB11`; common GND | Board B shows `NODE LOST` |
| ADC input | MQ AO never exceeds 3.3 V | Bad readings, or possible MCU damage |
| OLED | `PB6/PB7`, 3.3 V, GND connected | Blank OLED or garbled display |
| RGB LED | `PA1/PA2/PA3` not externally conflicted | Wrong alarm colors |
| Optional W25Q64 | 3.3 V and correct SPI2 wiring | JEDEC ID not detected, logging unavailable |

## 7. Code Mapping

| Hardware Layer | Code Location | Description |
|---|---|---|
| System clock | `SystemClock_Config()` | 8 MHz HSE to 72 MHz SYSCLK, APB1 36 MHz, ADC 12 MHz |
| On-board GPIO | `MX_GPIO_Init()`, `LED_Set()`, `Monitor_UpdateButtons()` | RGB LED and K1/K2 |
| USART1 debug | `USART1_Init_Custom()`, `_write()` | `printf` to CH340C |
| USART3 board link | `Node_USART3_Init()`, `USART3_IRQHandler()` | Board A sends frames, Board B receives bytes in interrupt |
| ADC | `ADC1_Init_Custom()`, `ADC1_ReadChannel()` | MQ135/MQ2 raw ADC sampling |
| DHT11 | `DHT11_Read()`, `DHT11_PERIOD_MS` | One-wire timing and safe sampling interval |
| OLED | `OLED_Init_Custom()`, `OLED_PrintLine()` | Bit-banged I2C SSD1306 driver |
| W25Q64 | `Flash_Init_Custom()`, `Flash_LogFrame()` | SPI2 JEDEC ID, sector erase, page program |

