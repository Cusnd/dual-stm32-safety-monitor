# SSD1306 OLED Module

[Hardware index](../index.en.md) | [中文](oled-ssd1306-module.zh-CN.md) | [SSD1306](../chips/ssd1306.en.md)

## Role

Board B uses a 128x64 SSD1306 I2C OLED for local state, readings, thresholds, and flash count display.

## Module Pins

| Module pin | Electrical role | Reference connection |
|---|---|---|
| `VCC` | Module supply | 3.3 V |
| `GND` | Ground | Common ground |
| `SCL` | I2C clock | `PB6`, bit-banged open-drain |
| `SDA` | I2C data | `PB7`, bit-banged open-drain |

## Electrical And Interface Notes

- Reference wiring uses 3.3 V pull-ups on `SCL/SDA`; do not pull either line to 5 V unless the target board is explicitly tolerant.
- Firmware uses a simple bit-banged I2C write-only path and the common 7-bit OLED address `0x3C`.
- Some modules expose address-select pads or reset pins; leave them at the module default unless the firmware is updated.

## Generic Reference Circuit

```text
3V3 -> VCC
PB6 -> SCL with pull-up
PB7 -> SDA with pull-up
GND -> GND
```

## Firmware Mapping

- `OLED_Init_Custom()` initializes SSD1306.
- `Monitor_UpdateDisplay()` writes four text lines.
- I2C address is the common `0x3C` module address.

## Fault Symptoms

- Blank screen: check address, power, SCL/SDA order, and pull-ups.
- Garbled text: check clock delay, ground, and module voltage.
