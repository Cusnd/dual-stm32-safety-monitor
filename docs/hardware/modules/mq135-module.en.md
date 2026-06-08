# MQ135 Module

[Hardware index](../index.en.md) | [中文](mq135-module.zh-CN.md) | [MQ135 element](../chips/mq135.en.md) | [LM393](../chips/lm393.en.md)

## Role

Board A uses MQ135 AO as a raw air-quality trend input. The module's comparator output is not used by the reference firmware.

## Module Pins

| Module pin | Electrical role | Reference connection |
|---|---|---|
| `VCC` | Heater/module supply | 5 V or module-rated supply |
| `GND` | Ground | Common ground |
| `AO` | Analog divider output | `PA4 / ADC1_CH4` |
| `DO` | Comparator output | Not connected |

## Electrical And Interface Notes

- The heater side may require 5 V and significant current; power it from a rail sized for the module.
- The STM32 ADC input must remain within 0..3.3 V even if the module is powered from 5 V.
- `AO` is a slow analog trend signal; `DO` is only a potentiometer/comparator threshold output and is not used by the firmware.

## Generic Reference Circuit

```text
Gas element + load resistor -> AO
AO -> PA4 ADC, protected to 0..3.3 V
AO + potentiometer -> LM393 -> DO
```

## Firmware Mapping

- `hal::readAdc1Channel(4)` samples MQ135.
- Threshold profiles use raw ADC counts.
- JSON field: `mq135Raw`.

## Test And Calibration

- Warm up before judging values.
- Record clean-air baseline and safe stimulus response.
- If AO can exceed 3.3 V, add a divider or clamp before `PA4`.
