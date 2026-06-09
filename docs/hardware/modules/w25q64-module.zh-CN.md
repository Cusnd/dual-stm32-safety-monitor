# W25Q64 模块

[硬件索引](../index.zh-CN.md) | [English](w25q64-module.en.md) | [W25Q64 芯片](../chips/w25q64.zh-CN.md)

## 项目角色

板 B 可选接入该模块作为 8 MB 环形历史日志。不接时显示和报警仍正常。

## 模块引脚

| 模块引脚 | 电气角色 | 参考 STM32 引脚 |
|---|---|---|
| `VCC` | 3.3 V 电源 | `3V3` |
| `GND` | 地 | `GND` |
| `CS` | SPI 片选 | `PB12` |
| `SCK/CLK` | SPI 时钟 | `PB13 / SPI2_SCK` |
| `SO/DO/MISO` | SPI 输出到 MCU | `PB14 / SPI2_MISO` |
| `SI/DI/MOSI` | SPI 从 MCU 输入 | `PB15 / SPI2_MOSI` |

## 电气与接口要点

- W25Q64 是 3.3 V SPI NOR Flash，不能用 5 V 供电或 5 V 信号驱动。
- 参考固件通过 SPI2 的 HAL SPI 使用标准单线 SPI 指令，不使用 Quad SPI。
- `CS` 按 GPIO 控制，便于 sector 0 元数据和 32 字节日志记录共用同一个 SPI2 外设并保持事务边界清晰。

## 通用参考电路

```text
3V3 -> VCC
PB12 -> CS
PB13 -> SCK
PB14 <- SO
PB15 -> SI
GND -> GND
```

## 固件映射

- JEDEC ID 容量字节应匹配 W25Q64（`0x17`）。
- 元数据扇区：sector 0。
- 日志区域：sector 1 到末尾，32 字节记录。
- JSON 字段：`flashReady`、`flashRecords`。

## 测试要点

- 通过标准：启动日志打印 `flash=ok`，随后打印 W25Q ID 和记录数；JSON 中 `flashReady=1`。
- 模块严禁接 5 V。
