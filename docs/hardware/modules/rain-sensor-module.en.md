# Rain Sensor Module

[Hardware index](../index.en.md) | [中文](rain-sensor-module.zh-CN.md) | [LM393](../chips/lm393.en.md)

## Role

Board A uses the rain sensor `SIG` analog output for wet/dry trend detection. Rain wet state is a warning-level event.

## Module Pins

| Module pin | Electrical role | Reference connection |
|---|---|---|
| `VCC` | Module supply | 3.3 V |
| `GND` | Ground | Common ground |
| `SIG` / `AO` | Analog rain plate signal | `PA6 / ADC1_CH6` |
| `DO` | Optional comparator output | Not used in this firmware |

## Electrical And Interface Notes

- The reference design powers the module from 3.3 V so `SIG/AO` naturally fits the STM32 ADC range.
- Wet/dry direction and threshold vary by module resistor network; calibrate from measured dry and wet ADC values.
- The optional `DO` pin is a comparator threshold signal and is intentionally unused in this firmware.

## Generic Reference Circuit

```text
Rain plate resistance -> analog divider -> SIG
SIG -> PA6 ADC
SIG + potentiometer -> optional LM393 DO
```

## Firmware Mapping

- `hal::readAdc1Channel(6)` samples rain.
- Default wet threshold starts at raw `1400`.
- JSON fields: `rainRaw`, `rainWet`; status bit: `bit2`.

## Calibration

- Record dry, misted, and wet ADC readings on the actual module.
- Move the wet threshold between the dry and wet values.
- Corrosion and residue change readings; clean the plate after tests.
