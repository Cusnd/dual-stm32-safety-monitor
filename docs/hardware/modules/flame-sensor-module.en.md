# Flame Sensor Module

[Hardware index](../index.en.md) | [Chinese](flame-sensor-module.zh-CN.md) | [LM393](../chips/lm393.en.md)

## Role

Board A uses the digital DO output to detect flame-like infrared intensity. Flame trigger is a danger-level event.

## Module Pins

| Module pin | Electrical role | Reference connection |
|---|---|---|
| `VCC` | Module supply | 3.3 V or module-rated supply |
| `GND` | Ground | Common ground |
| `DO` | Comparator output | `PB13`, active-low trigger |
| `AO` | Optional analog output | Not connected |

## Electrical And Interface Notes

- Reference firmware uses only `DO`; the LM393-style output is read with an internal pull-up and treated as active-low.
- `AO` is module-dependent and unused in this pin map; do not connect it to an ADC unless its range is verified within 0..3.3 V.
- Flame modules are threshold detectors for demos and trend monitoring, not certified fire-safety sensors.

## Generic Reference Circuit

```text
IR photodiode/transistor -> analog node
Analog node + potentiometer -> LM393 -> DO
DO -> PB13 input with pull-up
```

## Firmware Mapping

- `GPIO_PIN_RESET` on `PB13` means `flame=1`.
- Flame danger takes priority over warning states.
- JSON field: `flame`.

## Test Notes

- Use safe light/IR testing only; avoid exposing electronics to open flame.
- Adjust the module threshold before demos to avoid sunlight or remote-control false triggers.
