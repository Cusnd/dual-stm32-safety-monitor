# WS2813E RGB Module

[Hardware index](../index.en.md) | [中文](ws2813e-rgb-module.zh-CN.md) | [WS2813E chip](../chips/ws2813e.en.md)

## Role

Board B uses an external WS2813E RGB LED or strip as a high-visibility alarm indicator.

## Module Pins

| Module pin | Electrical role | Reference connection |
|---|---|---|
| `VCC` | LED power | 5 V |
| `GND` | Ground | Common with Board B |
| `DIN` | Data input | `PA6 / TIM3_CH1` |
| `DOUT` | Data output | Next LED, optional |

## Electrical And Interface Notes

- LED power is 5 V and can draw much more current than a GPIO; size the 5 V rail for the LED count and brightness.
- The data protocol is an approximately 800 kHz single-wire stream; firmware sends GRB bytes using TIM3 PWM plus DMA.
- A 3.3 V to 5 V level shifter on `DIN` is recommended for reliable margin, especially with longer wires or strips.

## Generic Reference Circuit

```text
5V -> VCC
GND -> GND and Board B GND
PA6 -> 3.3V-to-5V level shifter -> DIN
DOUT -> next DIN if strip continues
```

## Firmware Mapping

- `RGB_LED_COUNT` concept is implemented as `WS2813_LED_COUNT`.
- Default count is 1.
- GRB color order.
- JSON field: `externalRgb`.

## Test Notes

- Normal = green, warning = amber, danger = red, node lost = blue.
- If first LED works but later LEDs do not, check count, power injection, and DOUT chain.
