# DHT11 Module

[Hardware index](../index.en.md) | [中文](dht11-module.zh-CN.md) | [DHT11 chip](../chips/dht11.en.md)

## Role

Board A uses this module for ambient temperature and humidity. It is a low-speed digital sensor, so the firmware reads it less often than the one-second frame period.

## Module Pins

| Module pin | Electrical role | Reference STM32 pin |
|---|---|---|
| `VCC` | 3.3 V supply in this design | `3V3` |
| `DATA` | Bidirectional one-wire data | `PB12` |
| `NC` | Not connected | Floating |
| `GND` | Ground | `GND` |

## Electrical And Interface Notes

- `DATA` is a bidirectional single-wire signal and needs a pull-up to the module supply.
- The sensor returns a 40-bit humidity/temperature/checksum transaction after a host start pulse.
- Keep the sampling interval slow; the reference firmware waits at least `DHT11_PERIOD_MS` between reads.

## Generic Reference Circuit

```text
3V3 -> VCC
PB12 <-> DATA
DATA -> pull-up resistor -> 3V3
GND -> GND
```

## Firmware Mapping

- `DHT11_PORT/PIN` = `GPIOB/PB12`.
- `Dht11::read()` verifies the 40-bit checksum.
- `status bit0` reports read failure.

## Test And Faults

- Passing test: temperature/humidity update about every 3 seconds and `status bit0` is not stuck.
- If reads fail, check pull-up, cable length, DATA order, and common ground.
