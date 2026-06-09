# W25Q64 Module

[Hardware index](../index.en.md) | [Chinese](w25q64-module.zh-CN.md) | [W25Q64 chip](../chips/w25q64.en.md)

## Role

Board B optionally uses this module for the 8 MB circular history log. The monitor still works without it.

## Module Pins

| Module pin | Electrical role | Reference STM32 pin |
|---|---|---|
| `VCC` | 3.3 V supply | `3V3` |
| `GND` | Ground | `GND` |
| `CS` | SPI chip select | `PB12` |
| `SCK/CLK` | SPI clock | `PB13 / SPI2_SCK` |
| `SO/DO/MISO` | SPI data to MCU | `PB14 / SPI2_MISO` |
| `SI/DI/MOSI` | SPI data from MCU | `PB15 / SPI2_MOSI` |

## Electrical And Interface Notes

- W25Q64 is a 3.3 V SPI NOR flash; do not power or signal it at 5 V.
- The firmware uses standard single-SPI commands through HAL SPI on SPI2, not quad-SPI.
- `CS` is controlled as GPIO so metadata and 32-byte log records can share the same SPI2 peripheral with deterministic transactions.

## Generic Reference Circuit

```text
3V3 -> VCC
PB12 -> CS
PB13 -> SCK
PB14 <- SO
PB15 -> SI
GND -> GND
```

## Firmware Mapping

- JEDEC ID capacity byte must match W25Q64 (`0x17`).
- Metadata sector: sector 0.
- Log area: sector 1 to end, 32-byte records.
- JSON field: `flashReady`, `flashRecords`.

## Test Notes

- Passing test: boot log prints `flash=ok`, then W25Q ID and record count; JSON reports `flashReady=1`.
- Never power the module from 5 V.
