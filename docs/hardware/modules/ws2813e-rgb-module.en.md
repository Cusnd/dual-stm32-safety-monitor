# WS2813E RGB Module (Legacy Reference)

[Hardware index](../index.en.md) | [Chinese](ws2813e-rgb-module.zh-CN.md) | [WS2813E chip](../chips/ws2813e.en.md)

## Current Status

This module page is retained for hardware reference only. The active MONITOR firmware currently uses OLED, buzzer, buttons, optional W25Q64, and JSON output; it does not drive an external WS2813E RGB LED.

## Module Pins

| Module pin | Electrical role | Legacy reference connection |
|---|---|---|
| `VCC` | LED power | 5 V |
| `GND` | Ground | Common with Board B |
| `DIN` | Data input | `PA6 / TIM3_CH1` if firmware support is restored |
| `DOUT` | Data output | Next LED, optional |

## Electrical Notes

- LED power is 5 V and can draw much more current than a GPIO.
- A 3.3 V to 5 V level shifter on `DIN` is recommended for reliable margin.
- Keep this module disconnected from `PA6` in the current active firmware unless you intentionally restore support.

## Re-Enabling Checklist

- Restore firmware driver and CMake source registration.
- Define the active alarm-to-color mapping.
- Decide whether any JSON field such as `externalRgb` is needed; it is not required by the current dashboard schema.
- Update wiring, README, module index, and presentation material before treating this as an active feature.

