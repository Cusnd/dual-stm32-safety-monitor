# Active Buzzer Module

[Hardware index](../index.en.md) | [中文](active-buzzer-module.zh-CN.md)

## Role

Board B uses an active buzzer for local audible alarms. The firmware only turns it on/off; no audio PWM is required.

## Module Pins

| Module pin | Electrical role | Reference connection |
|---|---|---|
| `VCC` | Module supply | 3.3 V in the reference design |
| `GND` | Ground | Common ground |
| `SIG` | Enable input | `PB8`, active-high |

## Electrical And Interface Notes

- Reference signal level is 3.3 V GPIO; use a module with an onboard driver or add a transistor if buzzer current exceeds GPIO capability.
- The active buzzer contains the oscillator, so the interface is simple level control: `PB8=high` turns sound on.
- If a 5 V buzzer module is used, confirm `SIG` accepts a 3.3 V high level and does not drive 5 V back into `PB8`.

## Generic Reference Circuit

```text
3V3 -> VCC
PB8 -> driver/transistor input -> active buzzer
GND -> GND
```

## Firmware Mapping

- `Buzzer_Set()` drives `PB8`.
- `Monitor_UpdateAlarm()` selects cadence.
- K2 short press mutes buzzer only; LED states still show alarm state.

## Test Notes

- Danger state should produce fast beeps.
- Node-lost state should produce slow beeps.
- If silent, check whether mute is active before probing hardware.
