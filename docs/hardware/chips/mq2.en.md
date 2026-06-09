# MQ2 Gas Sensor Element

[Hardware index](../index.en.md) | [Chinese](mq2.zh-CN.md)

## Role

MQ2 is used as a smoke and combustible-gas trend sensor. The project treats high raw ADC values as warning or danger according to the active MQ2 level from the five-level threshold table.

## Element Pins

| Pin group | Function |
|---|---|
| `H/H` | Heater coil |
| `A/A` | One side of sensing resistor |
| `B/B` | Other side of sensing resistor |

Module boards usually expose `VCC`, `GND`, `AO`, and `DO`.

## Electrical Capability And Interface

- Responds to smoke and several combustible gases.
- Requires heater power and warm-up.
- Not selective enough for identifying a single gas in this firmware.

## Generic Reference Circuit

```text
5V module rail -> heater
Rsensor + RL -> AO
AO -> STM32 ADC, clamped/divided to <= 3.3 V
AO -> optional comparator threshold -> DO
```

## Firmware Mapping

- Reference AO connects to Board A `PA5 / ADC1_CH5`.
- `evaluateAlarm()` checks the active MQ2 danger threshold before warning logic.
- Default monitor level `3/5` uses warning `1800` and danger `2800`; `PB0` selects MQ2 and `PB1` cycles the level.
- Values are raw ADC counts, not gas concentration.

## Debugging

- If value never changes, check heater supply and AO wiring.
- If ADC saturates, add a divider or verify the module AO range.
