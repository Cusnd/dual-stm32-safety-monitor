# Module Reference

[English README](../README.md) | [中文](MODULE_REFERENCE.zh-CN.md) | [Wiring Guide](../WIRING.md) | [Board And Chip Reference](BOARD_AND_CHIP_REFERENCE.en.md) | [CLion + CMake](CLION_CMAKE_GUIDE.en.md)

This document describes the external modules used by the project. For each module it covers:

1. Function and project role.
2. Circuit and signal behavior.
3. Pin descriptions.
4. Actual project wiring and code mapping.
5. Power, wiring, and debugging notes.

Reference material includes Fire module manuals, schematics, wiring sheets, `Core/Src/main.c`, and `WIRING.md`.

## 1. Project Module Overview

| Module | Node | Project Role | Interface | Project Pins |
|---|---|---|---|---|
| DHT11 temperature/humidity module | Board A SENSOR | Temperature and humidity | One-wire digital signal | `PB12` |
| MQ135 air-quality module | Board A SENSOR | Air-quality trend detection | Analog ADC | `PA4 / ADC1_CH4` |
| MQ2 smoke module | Board A SENSOR | Smoke/combustible-gas trend detection | Analog ADC | `PA5 / ADC1_CH5` |
| Flame sensor module | Board A SENSOR | Flame or strong IR trigger | Digital DO | `PB13` |
| 0.96-inch IIC OLED | Board B MONITOR | Value, state, and threshold display | Software I2C | `PB6/PB7` |
| Active buzzer module | Board B MONITOR | Audible alarm | Digital output | `PB8` |
| W25Q64 Flash module | Board B MONITOR, optional | History logging | SPI2 | `PB12/PB13/PB14/PB15` |
| Board-to-board wires | Board A and Board B | Sensor frame transport | USART3 TTL serial | Cross `PB10/PB11` |

## 2. Wiring Summary

| Module | Module Pin | STM32 Pin / Power | Note |
|---|---|---|---|
| DHT11 | VCC | `3V3` | Fire DHT11 module is used at 3.3 V |
| DHT11 | DATA | `PB12` | One-wire bus; firmware switches output/input mode |
| DHT11 | NC | floating | Not connected |
| DHT11 | GND | `GND` | Common ground |
| MQ135 | VCC | `5V` | Heated gas sensor, requires warm-up |
| MQ135 | AO | `PA4` | Analog output; must not exceed 3.3 V |
| MQ135 | DO | not connected | Comparator digital output is not used |
| MQ135 | GND | `GND` | Common ground |
| MQ2 | VCC | `5V` | Heated gas sensor, requires warm-up |
| MQ2 | AO | `PA5` | Analog output; must not exceed 3.3 V |
| MQ2 | DO | not connected | Comparator digital output is not used |
| MQ2 | GND | `GND` | Common ground |
| Flame module | VCC | `3V3/5V` | Follow module requirements; DO must be STM32-safe |
| Flame module | DO | `PB13` | Active-low trigger |
| Flame module | AO | not connected | Analog output is not used |
| Flame module | GND | `GND` | Common ground |
| OLED | VCC | `3V3` | SSD1306 IIC OLED |
| OLED | SCL | `PB6` | Software I2C clock |
| OLED | SDA | `PB7` | Software I2C data |
| OLED | GND | `GND` | Common ground |
| Active buzzer | VCC | `3V3` | Do not power the Fire active buzzer module from 5 V |
| Active buzzer | SIG | `PB8` | Active high |
| Active buzzer | GND | `GND` | Common ground |
| W25Q64 | VCC | `3V3` | Do not use 5 V |
| W25Q64 | CS | `PB12` | SPI2 chip select |
| W25Q64 | CLK/SCK | `PB13` | SPI2_SCK |
| W25Q64 | DO/SO/MISO | `PB14` | SPI2_MISO |
| W25Q64 | DI/SI/MOSI | `PB15` | SPI2_MOSI |
| W25Q64 | GND | `GND` | Common ground |

## 3. DHT11 Temperature/Humidity Module

### 3.1 Module Description

DHT11 is a low-speed digital temperature/humidity sensor. It contains a humidity element, an NTC temperature element, and an internal 8-bit controller. It is not UART, I2C, or SPI; it uses a one-wire protocol and requires only one MCU IO.

| Parameter | Description |
|---|---|
| Supply | 3.3 V to 5.5 V; the Fire module connector is labeled 3V3 |
| Humidity range | 5-95%RH |
| Humidity accuracy | About +/-5%RH |
| Temperature range | -20 to 60 C |
| Temperature accuracy | About +/-2 C |
| Data width | 40 bits: humidity integer/decimal, temperature integer/decimal, checksum |
| Sampling interval | More than 2 seconds between readings |

### 3.2 Circuit Behavior

DHT11 DATA idles high through a pull-up resistor. The MCU starts a read by pulling DATA low for more than 18 ms, releases the bus, then DHT11 responds with pulses and 40 bits of data.

The project switches `PB12` between modes:

| Stage | GPIO Mode | Purpose |
|---|---|---|
| Start signal | Open-drain output | MCU pulls DATA low for about 20 ms |
| Bus release | Open-drain high, then input | Pull-up brings DATA high |
| Receive data | Pull-up input | DHT11 drives 40 bits |

### 3.3 Project Wiring And Code

| Item | Value |
|---|---|
| Node | Board A SENSOR |
| DATA | `PB12` |
| Supply | `3V3` |
| Code | `DHT11_Read()` |
| Sampling control | `DHT11_PERIOD_MS = 2100` |
| Error reporting | `STATUS_DHT_ERROR` in frame `status` |

Board A sends one frame per second, but it does not read DHT11 every second. The firmware guarantees at least 2100 ms between DHT11 reads; with a 1000 ms frame period, temperature/humidity updates about every 3 seconds.

### 3.4 Notes

- Do not read DHT11 continuously as a high-speed sensor.
- Long wires can disturb the one-wire bus; use short wiring and common ground.
- DHT11 is suitable for demos, not precision environmental measurement.

## 4. MQ135 Air-Quality Module

### 4.1 Module Description

MQ135 is a heated gas-sensor module often used for air-quality trend detection. It responds to gases such as ammonia, sulfides, and benzene-family vapors. This project does not perform ppm calibration; it uses raw ADC values for trends and thresholds.

| Parameter | Description |
|---|---|
| Recommended supply | 5 V |
| Outputs | AO analog output, DO comparator output |
| Project usage | AO analog output |
| Warm-up | At least 5 minutes recommended; longer warm-up improves stability |
| Calibration | Required for ppm conversion |

### 4.2 Circuit Behavior

MQ135 modules usually contain the MQ135 sensing element, heater circuit, load resistor, LM393 comparator, and potentiometer:

| Circuit Part | Purpose |
|---|---|
| Heater | Brings the sensitive material to operating temperature |
| Sensing resistor | Resistance changes with gas concentration |
| AO | Analog voltage from the sensing divider |
| LM393 comparator | Compares AO against the potentiometer threshold |
| Potentiometer | Adjusts DO trigger level |

The project reads only AO. The DO threshold potentiometer does not affect the MCU ADC reading.

### 4.3 Project Wiring And Code

| Item | Value |
|---|---|
| Node | Board A SENSOR |
| AO | `PA4 / ADC1_CH4` |
| VCC | `5V` |
| GND | `GND` |
| Code | `ADC1_ReadChannel(4)` |
| Filtering | Exponential moving average: `mq135_avg = (old * 3 + raw) / 4` |
| Alarm use | Board B compares against threshold profiles |

### 4.4 Notes

- `PA4` is an ADC input. AO must never exceed 3.3 V.
- If AO can exceed 3.3 V, add a divider or clamp circuit.
- Displayed values are 12-bit raw ADC readings, not ppm.

## 5. MQ2 Smoke Module

### 5.1 Module Description

MQ2 is a heated smoke and combustible-gas sensor module. It responds to smoke, propane, methane, ethanol, natural gas, and similar gases. This project uses it for smoke/combustible-gas trend detection and danger alarms.

| Parameter | Description |
|---|---|
| Recommended supply | 5 V |
| Outputs | AO analog output, DO comparator output |
| Project usage | AO analog output |
| Warm-up | At least 5 minutes recommended |
| Calibration | Current project does not calculate ppm; thresholds must be tuned on-site |

### 5.2 Circuit Behavior

The MQ2 circuit is similar to MQ135: heater, sensing resistor, analog divider output, LM393 comparator, and potentiometer. AO reflects analog voltage; DO reflects the comparator threshold result.

The project assigns two ADC channels:

| Module | ADC Channel | Reason |
|---|---|---|
| MQ135 | `PA4 / ADC1_CH4` | Air-quality trend |
| MQ2 | `PA5 / ADC1_CH5` | Smoke/combustible-gas trend |

### 5.3 Project Wiring And Code

| Item | Value |
|---|---|
| Node | Board A SENSOR |
| AO | `PA5 / ADC1_CH5` |
| VCC | `5V` |
| GND | `GND` |
| Code | `ADC1_ReadChannel(5)` |
| Filtering | Exponential moving average: `mq2_avg = (old * 3 + raw) / 4` |
| Alarm use | Board B compares against warning/danger thresholds |

### 5.4 Notes

- `PA5` is an ADC input. AO must stay within 0-3.3 V.
- MQ2 heater current is not tiny; use a stable 5 V source.
- Use safe demo sources; avoid open flame near wiring.

## 6. Flame Sensor Module

### 6.1 Module Description

The flame sensor module detects flame or strong infrared light, typically around 760-1100 nm. It usually provides AO analog output and DO digital output.

| Parameter | Description |
|---|---|
| Supply | 3.3 V or 5 V, depending on module use |
| Outputs | AO analog output, DO digital output |
| Project usage | DO digital output |
| Trigger polarity | DO goes low when flame/IR exceeds threshold |
| Threshold | Adjusted by module potentiometer |

### 6.2 Circuit Behavior

The module usually contains an IR receiver, comparator, and potentiometer. Fire documentation describes DO as:

| State | DO |
|---|---|
| Below threshold | High |
| Flame/strong IR above threshold | Low |

### 6.3 Project Wiring And Code

| Item | Value |
|---|---|
| Node | Board A SENSOR |
| DO | `PB13` |
| VCC | `3V3/5V`, according to module requirements |
| GND | `GND` |
| Code read | `HAL_GPIO_ReadPin(FLAME_PORT, FLAME_PIN)` |
| Trigger logic | `GPIO_PIN_RESET` means `flame = 1` |
| Alarm priority | Flame trigger is danger: red LED and fast buzzer |

### 6.4 Notes

- If the module uses 5 V supply, confirm DO is safe for STM32 digital input.
- `PB13` is a 5V-tolerant digital IO on STM32F103, but ADC pins are not 5V tolerant.
- Sunlight and strong IR remotes can trigger the module; tune the potentiometer before demos.

## 7. 0.96-Inch IIC OLED Module

### 7.1 Module Description

The project uses a 0.96-inch IIC OLED with SSD1306 controller and 128x64 resolution. The firmware does not enable STM32 hardware I2C; it bit-bangs I2C on `PB6/PB7`.

| Parameter | Description |
|---|---|
| Resolution | 128 x 64 |
| Controller | SSD1306 |
| Interface | I2C |
| Supply | 3.3 V |
| Address | 7-bit `0x3C`, transmitted byte address `0x78` |

### 7.2 Circuit Behavior

The OLED exposes VCC, GND, SCL, and SDA. I2C idles high and needs pull-ups. The Fire OLED module includes pull-ups; firmware also configures `PB6/PB7` as open-drain outputs with pull-up.

### 7.3 Project Wiring And Code

| Item | Value |
|---|---|
| Node | Board B MONITOR |
| SCL | `PB6` |
| SDA | `PB7` |
| VCC | `3V3` |
| GND | `GND` |
| Code | `OLED_Init_Custom()`, `OLED_Command()`, `OLED_Data()` |
| Refresh | `Monitor_UpdateDisplay()` |

### 7.4 Notes

- The expansion-board OLED socket maps directly to `PB6/PB7`.
- Some Fire OLED examples may use `PB10/PB11`; do not follow that wiring here because `PB10/PB11` are USART3.
- The current software-I2C driver does not check ACK, so an unplugged or wrong-address OLED will simply stay blank.

## 8. Active Buzzer Module

### 8.1 Module Description

An active buzzer has an internal oscillator/driver. The MCU only needs to drive SIG high or low; no PWM is required. The project uses it for local alarm sound.

| Parameter | Description |
|---|---|
| Type | Active buzzer |
| Supply | Fire module is used at 3.3 V in this project |
| Control | SIG high means sound on |
| Project pin | `PB8` |

### 8.2 Circuit Behavior

The module generally contains the buzzer and a transistor driver. When the MCU drives SIG high, the transistor turns on and the buzzer sounds. SIG low turns it off.

### 8.3 Project Wiring And Code

| Item | Value |
|---|---|
| Node | Board B MONITOR |
| SIG | `PB8` |
| VCC | `3V3` |
| GND | `GND` |
| Code | `Buzzer_Set()` |
| Pattern control | `Monitor_UpdateAlarm()` based on normal/warn/danger/node-lost states |

### 8.4 Notes

- Do not power the Fire active buzzer module from 5 V.
- If using a different buzzer, confirm it is active. A passive buzzer requires PWM and is not the default design.

## 9. W25Q64 Flash Module

### 9.1 Module Description

W25Q64 is a 64 Mbit SPI NOR Flash, equal to 8 MByte. The project uses it as optional history storage on Board B. Monitoring, display, and alarms still work without it.

| Parameter | Description |
|---|---|
| Capacity | 64 Mbit = 8 MByte |
| Supply | 2.7-3.6 V, project uses 3.3 V |
| Interface | Standard SPI |
| Page size | 256 Byte |
| Sector size | 4 KB |
| Common commands | JEDEC ID `0x9F`, read status `0x05`, write enable `0x06`, page program `0x02`, sector erase `0x20` |
| Typical JEDEC ID | Winbond often reports `EF 40 17` |

### 9.2 Circuit Behavior

The module exposes standard SPI lines:

| Module Pin | Description |
|---|---|
| CS | Chip select, active low, usually pulled up on the module |
| CLK/SCK | SPI clock |
| DI/SI/MOSI | Master output, slave input |
| DO/SO/MISO | Master input, slave output |
| VCC | 3.3 V power |
| GND | Ground |

WP and HOLD are usually pulled up on the module and not used. The project uses standard single-lane SPI, not Dual/Quad SPI.

### 9.3 Project Wiring And Code

| Item | Value |
|---|---|
| Node | Board B MONITOR, optional |
| CS | `PB12` |
| SCK | `PB13` |
| MISO | `PB14` |
| MOSI | `PB15` |
| VCC | `3V3` |
| GND | `GND` |
| Code | `Flash_Init_Custom()`, `Flash_LogFrame()` |
| Detection | Read `0x9F` JEDEC ID |
| Logging | Erase sector 0 at startup, then write rolling 20-byte records |

### 9.4 Expansion-Board Relationship

The expansion-board W25Q64/SPI1 direct socket maps to:

```text
PA4 / PA5 / PA6 / PA7
```

This project uses:

```text
PB12 / PB13 / PB14 / PB15
```

So do not plug W25Q64 directly into the expansion-board SPI1 socket for this firmware. Use jumper wires to SPI2 instead. This is because `PA4/PA5` are already assigned to MQ135/MQ2 ADC inputs.

### 9.5 Notes

- Never power W25Q64 from 5 V.
- Firmware erases Flash sector 0 during initialization. Do not connect a module that contains important data.
- The expansion board has a PB15 key on the same net; do not press it while using W25Q64 MOSI.
- The current SPI implementation is a demo-level polling driver. It is enough for this project, but production code should add fuller timeout/error recovery.

## 10. Board-To-Board USART3 Link

### 10.1 Wiring

The boards do not communicate through Wi-Fi or USB. They use direct MCU USART3 TTL serial. TX/RX must be crossed and ground must be shared.

| Board A SENSOR | Board B MONITOR | Description |
|---|---|---|
| `PB10 / USART3_TX` | `PB11 / USART3_RX` | Board A sends sensor frames to Board B |
| `PB11 / USART3_RX` | `PB10 / USART3_TX` | Reserved for future ACK/heartbeat |
| `GND` | `GND` | Required common reference |

Serial settings:

```text
115200 8N1
```

### 10.2 Frame Format

Board A sends one 13-byte frame per second:

```text
AA 55 LEN TEMP HUMI MQ135_H MQ135_L MQ2_H MQ2_L FLAME SEQ STATUS CHECKSUM
```

| Field | Description |
|---|---|
| `AA 55` | Frame header for resynchronization |
| `LEN` | Payload length, fixed to `9` |
| `TEMP/HUMI` | DHT11 integer temperature/humidity |
| `MQ135_H/L` | MQ135 12-bit ADC value, high byte first |
| `MQ2_H/L` | MQ2 12-bit ADC value, high byte first |
| `FLAME` | `1` means flame triggered |
| `SEQ` | Frame sequence number |
| `STATUS` | `bit0 = DHT11 read error` |
| `CHECKSUM` | Low 8 bits of `LEN + payload` |

### 10.3 Debug Symptoms

| Symptom | Likely Cause |
|---|---|
| Board A prints `[SENSOR]`, Board B stays `NODE LOST` | USART3 not crossed, no common GND, or Board B flashed with wrong firmware |
| Board B occasionally drops frames but recovers | Serial noise or loose wiring; frame-header resync handles recovery |
| Board B OLED values do not change | Board A not sending, serial link disconnected, or checksum fails |
| Board A has no logs | USART1 debug serial not connected or wrong baud rate |

## 11. Fire Example Wiring vs Project Wiring

| Module | Common Fire Example Wiring | Project Wiring | Reason |
|---|---|---|---|
| DHT11 | `PB0` or `PB12` | `PB12` | Matches current firmware `DHT11_PORT/PIN` |
| OLED | `PB10/PB11` or expansion-board `PB6/PB7` | `PB6/PB7` | `PB10/PB11` are USART3 |
| Buzzer | `PA6`, `PB9`, etc. | `PB8` | Board B alarm output |
| Flame DO | `PA11`, `PA5`, etc. | `PB13` | `PA11/PA12` are USB; `PA5` is MQ2 ADC |
| MQ135 AO | `PA4` | `PA4` | Same as example |
| MQ2 AO | Often also defaults to `PA4` | `PA5` | Two MQ modules need separate ADC channels |
| W25Q64 | SPI1 `PA4/PA5/PA6/PA7` | SPI2 `PB12/PB13/PB14/PB15` | `PA4/PA5` are MQ ADC pins |

## 12. Single-Module Test Suggestions

| Test Target | Suggested Step | Pass Criteria |
|---|---|---|
| DHT11 | Watch Board A `[SENSOR]` logs after power-up | Temperature/humidity updates about every 3 seconds; `status` is not stuck at `0x01` |
| MQ135 | Warm up, then expose to a safe air-quality change | `mq135` ADC value changes clearly |
| MQ2 | Warm up, then use a safe smoke/alcohol vapor source | `mq2` ADC value changes clearly |
| Flame module | Tune potentiometer and use a safe light/flame test source | `flame=1`, Board B enters danger |
| OLED | Flash Board B firmware and power up | OLED shows state/value pages |
| Buzzer | Trigger danger or node-lost | Buzzer follows alarm pattern |
| W25Q64 | Wire SPI2 and power up | Log prints W25Q ID; system still works without the module |
| USART3 | Cross two-board wires and share GND | Board B prints `[MONITOR] rx`, OLED values update |

