# MQ2 Module

[Hardware index](../index.en.md) | [中文](mq2-module.zh-CN.md) | [MQ2 element](../chips/mq2.en.md) | [LM393](../chips/lm393.en.md)

## Role

Board A uses MQ2 AO for smoke/combustible-gas trend monitoring. Board B treats high raw values as warning or danger.

## Module Pins

| Module pin | Electrical role | Reference connection |
|---|---|---|
| `VCC` | Heater/module supply | 5 V or module-rated supply |
| `GND` | Ground | Common ground |
| `AO` | Analog output | `PA5 / ADC1_CH5` |
| `DO` | Comparator output | Not connected |

## Electrical And Interface Notes

- MQ2 requires heater warm-up before readings are meaningful.
- The STM32 ADC input must stay within 0..3.3 V; add a divider or clamp if the module AO follows a 5 V rail.
- `AO` is sampled as a raw trend count; the optional `DO` comparator output is not connected in the reference firmware.

## Generic Reference Circuit

```text
MQ2 element divider -> AO
AO -> PA5 ADC, limited to 0..3.3 V
AO + potentiometer -> LM393 -> optional DO
```

## Firmware Mapping

- `ADC1_ReadChannel(5)` samples MQ2.
- `smoke_warn` and `smoke_danger` thresholds come from the active profile.
- JSON field: `mq2Raw`.

## Test And Faults

- Warm up the heater, then verify ADC response with a safe test source.
- Saturated ADC suggests AO over-range or wrong module supply.
