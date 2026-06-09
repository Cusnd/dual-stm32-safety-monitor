# MQ2 模块

[硬件索引](../index.zh-CN.md) | [English](mq2-module.en.md) | [MQ2 元件](../chips/mq2.zh-CN.md) | [LM393](../chips/lm393.zh-CN.md)

## 项目角色

板 A 使用 MQ2 AO 监测烟雾/可燃气体趋势。板 B 按 ADC 原始值进入预警或危险。

## 模块引脚

| 模块引脚 | 电气角色 | 参考连接 |
|---|---|---|
| `VCC` | 加热/模块供电 | 5 V 或模块额定电源 |
| `GND` | 地 | 共地 |
| `AO` | 模拟输出 | `PA5 / ADC1_CH5` |
| `DO` | 比较器输出 | 不接 |

## 电气与接口要点

- MQ2 需要预热，加热稳定前读数只适合观察趋势。
- STM32 ADC 输入必须保持在 0..3.3 V；若模块 `AO` 跟随 5 V 电源，需要加分压或钳位。
- `AO` 作为原始趋势值采样；可选 `DO` 比较器输出在参考固件中不连接。

## 通用参考电路

```text
MQ2 元件分压 -> AO
AO -> PA5 ADC，限制在 0..3.3 V
AO + 电位器 -> LM393 -> 可选 DO
```

## 固件映射

- `hal::readAdc1Channel(5)` 采样 MQ2。
- 板 B 使用 5 档可调 MQ2 阈值；默认显示为 `3/5`，预警 `1800`、危险 `2800`。
- PB0 可在 MONITOR 阈值页选择 MQ2，PB1 让 `thresholdSmokeLevel` 在 `0..4` 间循环。
- JSON 字段：`mq2Raw`、`thresholdSmokeLevel`、`thresholdSmokeWarn`、`thresholdSmokeDanger`。

## 测试与故障

- 加热预热后，用安全测试源验证 ADC 响应。
- ADC 饱和通常表示 AO 超范围或模块供电/分压不合适。
