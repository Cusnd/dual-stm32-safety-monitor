# DHT11 Sensor IC

[Hardware index](../index.en.md) | [中文](dht11.zh-CN.md)

## Role

DHT11 provides low-cost digital temperature and humidity readings on Board A. The project reads it at a safe interval and reports failures through `STATUS_DHT_ERROR`.

## Pins

| Pin | Function | Reference connection |
|---|---|---|
| `VDD` | Power | 3.3 V in the reference module |
| `DATA` | Bidirectional one-wire signal | Board A `PB12` |
| `NC` | No connect | Leave floating |
| `GND` | Ground | Common ground |

## Electrical Capability And Interface

- Digital one-wire style protocol.
- 40-bit transfer: humidity, temperature, and checksum bytes.
- Requires slow sampling; the firmware waits at least 2100 ms between reads.

## Generic Reference Circuit

```text
3V3 -> VDD
PB12 <-> DATA with pull-up to 3V3
GND -> GND
```

## Firmware Mapping

- `Dht11::read()` drives the start pulse, switches the GPIO to input, reads 40 bits, and verifies checksum.
- `DHT11_PERIOD_MS` controls the safe sampling interval.
- Failure sets `status bit0`.

## Fault Symptoms

- Long-term `status bit0`: check pull-up, wiring order, and sampling interval.
- Constant zero or stale values: check module power and DATA direction switching.
