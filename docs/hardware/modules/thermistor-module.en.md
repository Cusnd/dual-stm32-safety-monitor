# Thermistor Module

[Hardware index](../index.en.md) | [中文](thermistor-module.zh-CN.md) | [10K NTC](../chips/10k-ntc-b3950.en.md) | [LM393](../chips/lm393.en.md)

## Role

Board A uses thermistor AO for temperature trend and DO for hardware high-temperature trigger. High temperature is a danger-level event.

## Module Pins

| Module pin | Electrical role | Reference connection |
|---|---|---|
| `VCC` | Module supply | 3.3 V |
| `GND` | Ground | Common ground |
| `AO` | Analog divider output | `PA7 / ADC1_CH7` |
| `DO` | Comparator output | `PB9`, pull-up input, active-low high temperature |

## Electrical And Interface Notes

- Reference conversion assumes a 10K NTC with B=3950 and a divider that produces a valid 0..3.3 V ADC signal.
- `DO` is a module comparator output; the firmware reads it with pull-up and treats low level as hot.
- If the module resistor value differs from 10K or the divider orientation is reversed, update the lookup table/interpretation after measurement.

## Generic Reference Circuit

```text
10K NTC + fixed resistor -> AO divider
AO -> PA7 ADC
AO + potentiometer -> LM393 -> DO
```

## Firmware Mapping

- `Thermistor_AdcToC10()` converts AO to 0.1 deg C.
- Warning/danger defaults: 45.0 deg C / 70.0 deg C.
- JSON fields: `thermRaw`, `thermC10`, `thermHot`; status bits: `bit1`, `bit3`.

## Test And Calibration

- Warm the probe safely; `thermC10` should rise.
- Adjust module potentiometer and verify `thermHot=1` when DO triggers.
- If `bit3` persists, check open/short circuit and AO range.
