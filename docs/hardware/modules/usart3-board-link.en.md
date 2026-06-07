# USART3 Board-to-Board Link

[Hardware index](../index.en.md) | [中文](usart3-board-link.zh-CN.md) | [STM32F103C8T6](../chips/stm32f103c8t6.en.md)

## Role

The two STM32 boards communicate over TTL USART3. Board A sends sensor frames to Board B; the reverse direction is reserved for future ACK or commands.

## Reference Wiring

| Board A SENSOR | Board B MONITOR | Purpose |
|---|---|---|
| `PB10 / USART3_TX` | `PB11 / USART3_RX` | Sensor frames to monitor |
| `PB11 / USART3_RX` | `PB10 / USART3_TX` | Reserved reverse path |
| `GND` | `GND` | Required common reference |

## Connector Pins And Protocol

- Link level is 3.3 V TTL UART at `115200 8N1`.
- Board A periodically sends framed binary v2 sensor packets; Board B validates header, length, version, checksum, and sequence.
- The reverse path is wired for future commands but is not required for the current monitoring workflow.

## Generic Reference Circuit

```text
Board A PB10/TX ----> Board B PB11/RX
Board A PB11/RX <---- Board B PB10/TX
Board A GND   ------- Board B GND
```

## Electrical Notes

- UART level is 3.3 V TTL, not RS-232.
- Do not insert a USB-UART bridge between the two boards unless it preserves level and direction.
- Keep wires short for reliability.

## Firmware Mapping

- `Node_USART3_Init()` configures `115200 8N1`.
- Board B uses an interrupt ring buffer and validates v2 frame length/checksum.

## Fault Symptoms

- Board B shows `NODE LOST`: check crossed TX/RX, common GND, and firmware roles.
- Intermittent frames: check loose wires and ground noise.
