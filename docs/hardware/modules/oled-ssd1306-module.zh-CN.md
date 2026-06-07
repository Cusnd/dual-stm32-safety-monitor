# SSD1306 OLED 模块

[硬件索引](../index.zh-CN.md) | [English](oled-ssd1306-module.en.md) | [SSD1306](../chips/ssd1306.zh-CN.md)

## 项目角色

板 B 使用 128x64 SSD1306 I2C OLED 显示本地状态、读数、阈值和 Flash 记录数。

## 模块引脚

| 模块引脚 | 电气角色 | 参考连接 |
|---|---|---|
| `VCC` | 模块供电 | 3.3 V |
| `GND` | 地 | 共地 |
| `SCL` | I2C 时钟 | `PB6`，软件开漏 |
| `SDA` | I2C 数据 | `PB7`，软件开漏 |

## 电气与接口要点

- 参考接线把 `SCL/SDA` 上拉到 3.3 V；除非目标板明确可承受，否则不要上拉到 5 V。
- 固件使用简单的软件 I2C 只写路径，常见 7 位 OLED 地址为 `0x3C`。
- 部分模块提供地址选择焊盘或复位脚；除非同步修改固件，否则保持模块默认配置。

## 通用参考电路

```text
3V3 -> VCC
PB6 -> SCL，并上拉
PB7 -> SDA，并上拉
GND -> GND
```

## 固件映射

- `OLED_Init_Custom()` 初始化 SSD1306。
- `Monitor_UpdateDisplay()` 写 4 行文本。
- I2C 地址使用常见模块地址 `0x3C`。

## 故障现象

- 空白屏：检查地址、供电、SCL/SDA 顺序和上拉。
- 乱码：检查时钟延时、共地和模块电压。
