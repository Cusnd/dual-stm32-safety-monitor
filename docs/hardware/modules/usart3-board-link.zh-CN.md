# USART3 双板链路

[硬件索引](../index.zh-CN.md) | [English](usart3-board-link.en.md) | [STM32F103C8T6](../chips/stm32f103c8t6.zh-CN.md)

## 项目角色

两块 STM32 板通过 TTL USART3 通信。板 A 向板 B 发送传感器帧；反向通道预留给后续 ACK 或命令。

## 参考接线

| 板 A SENSOR | 板 B MONITOR | 用途 |
|---|---|---|
| `PB10 / USART3_TX` | `PB11 / USART3_RX` | 采集帧发往显示节点 |
| `PB11 / USART3_RX` | `PB10 / USART3_TX` | 预留反向通道 |
| `GND` | `GND` | 必须共地 |

## 连接引脚与协议

- 链路电平为 3.3 V TTL UART，参数 `115200 8N1`。
- 板 A 周期发送 v2 二进制传感器帧；板 B 校验帧头、长度、版本、checksum 和序号。
- 反向链路为未来命令预留，当前监测流程不依赖它。

## 通用参考电路

```text
板 A PB10/TX ----> 板 B PB11/RX
板 A PB11/RX <---- 板 B PB10/TX
板 A GND   ------- 板 B GND
```

## 电气注意事项

- UART 电平是 3.3 V TTL，不是 RS-232。
- 两板之间不要插入会改变方向或电平的 USB 转串口桥，除非你明确设计了转接电路。
- 通信线尽量短，可靠性更好。

## 固件映射

- `hal::initNodeUsart3()` 配置 `115200 8N1`。
- 板 B 用中断环形缓冲，并校验 v2 帧长度和 checksum。

## 故障现象

- 板 B 显示 `NODE LOST`：检查 TX/RX 交叉、共地和两个固件角色。
- 偶发丢帧：检查杜邦线松动和地线噪声。
