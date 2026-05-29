# Wiring Guide / 接线说明

This guide lists the wiring needed to run the dual-board STM32 safety monitor.

本文档列出双 STM32 环境安全监测系统运行所需的接线。

## Reserved Board Pins / 板载保留引脚

Do not reuse these pins for external modules.

以下引脚已经承担板载功能，不建议再分配给外部模块。

| Pin / 引脚 | Board function / 板载功能 | Project use / 项目用途 |
|---|---|---|
| `PA9` | USART1_TX to CH340C / 接 CH340C | USB serial debug / USB 串口调试 |
| `PA10` | USART1_RX to CH340C / 接 CH340C | USB serial debug / USB 串口调试 |
| `PA11/PA12` | USB Device / USB Device 接口 | Reserved / 保留 |
| `PA13/PA14` | SWD / SWD 下载调试 | Programming/debug / 下载和调试 |
| `PA1/PA2/PA3` | On-board RGB LED / 板载 RGB LED | Monitor alarm LED / 显示节点报警灯 |
| `PA0` | K1 / 板载 K1 | OLED page switch / OLED 页面切换 |
| `PC13` | K2 / 板载 K2 | Mute / threshold profile / 静音与阈值档位 |

## Board-to-Board USART3 Link / 双板 USART3 连接

TX and RX must be crossed, and both boards must share GND.

两块板通信时 TX/RX 必须交叉连接，并且必须共地。

| Board A SENSOR / 板 A 采集节点 | Board B MONITOR / 板 B 显示节点 | Note / 说明 |
|---|---|---|
| `PB10 / USART3_TX` | `PB11 / USART3_RX` | Sensor data to monitor / 采集数据发送到显示节点 |
| `PB11 / USART3_RX` | `PB10 / USART3_TX` | Reserved for future ACK/heartbeat / 预留给 ACK 或心跳 |
| `GND` | `GND` | Required common reference / 必须共地 |

Serial settings: `115200 8N1`.

串口参数：`115200 8N1`。

## Board A SENSOR Wiring / 板 A 采集节点接线

| Module / 模块 | Module pin / 模块引脚 | STM32 pin / STM32 引脚 | Note / 说明 |
|---|---|---|---|
| DHT11 | DATA | `PB12` | Single-wire data / 单总线数据 |
| DHT11 | VCC | `3V3` | Follow your module label / 按模块标注供电 |
| DHT11 | GND | `GND` | Common ground / 共地 |
| MQ135 | AO | `PA4` | ADC1_CH4 |
| MQ135 | VCC | `5V` or module supply / 5V 或模块要求电压 | Ensure AO does not exceed 3.3V / 确保 AO 不超过 3.3V |
| MQ135 | GND | `GND` | Common ground / 共地 |
| MQ2 | AO | `PA5` | ADC1_CH5 |
| MQ2 | VCC | `5V` or module supply / 5V 或模块要求电压 | Ensure AO does not exceed 3.3V / 确保 AO 不超过 3.3V |
| MQ2 | GND | `GND` | Common ground / 共地 |
| Flame sensor / 火焰传感器 | DO | `PB13` | Active-low trigger / 低电平触发 |
| Flame sensor / 火焰传感器 | VCC | `3V3/5V` as required / 按模块要求 | Ensure logic output is 3.3V-compatible / 确保输出电平兼容 3.3V |
| Flame sensor / 火焰传感器 | GND | `GND` | Common ground / 共地 |

## Board B MONITOR Wiring / 板 B 显示报警节点接线

| Module / 模块 | Module pin / 模块引脚 | STM32 pin / STM32 引脚 | Note / 说明 |
|---|---|---|---|
| OLED | SCL | `PB6` | Software I2C clock / 软件 I2C 时钟 |
| OLED | SDA | `PB7` | Software I2C data / 软件 I2C 数据 |
| OLED | VCC | `3V3` | SSD1306 I2C OLED |
| OLED | GND | `GND` | Common ground / 共地 |
| Active buzzer / 有源蜂鸣器 | SIG | `PB8` | Active-high / 高电平响 |
| Active buzzer / 有源蜂鸣器 | VCC | `3V3/5V` as required / 按模块要求 | Active buzzer recommended / 推荐使用有源蜂鸣器 |
| Active buzzer / 有源蜂鸣器 | GND | `GND` | Common ground / 共地 |

## Optional W25Q64 / 可选 W25Q64

Connect this module only to Board B. The system still works without it.

该模块只连接到板 B；不接 W25Q64 时系统仍可正常监测和报警。

| W25Q64 pin / 引脚 | STM32 pin / STM32 引脚 | Note / 说明 |
|---|---|---|
| `CS` | `PB12` | Chip select / 片选 |
| `SCK` | `PB13` | SPI2_SCK |
| `SO / MISO` | `PB14` | SPI2_MISO |
| `SI / MOSI` | `PB15` | SPI2_MOSI |
| `VCC` | `3V3` | Do not use 5V / 不要接 5V |
| `GND` | `GND` | Common ground / 共地 |

## Power-On Checks / 上电检查

1. Check common GND between both boards. / 检查两块板是否共地。
2. Check Board A `PB10` to Board B `PB11`. / 检查板 A `PB10` 是否接到板 B `PB11`。
3. Check Board B `PB10` to Board A `PB11`. / 检查板 B `PB10` 是否接到板 A `PB11`。
4. Leave `PA9/PA10` free for USB serial debug. / 保留 `PA9/PA10` 给 USB 串口调试。
5. Open both serial ports at `115200 8N1`. / 以 `115200 8N1` 打开两个串口。
6. Board A should print `[SENSOR]` logs. / 板 A 应输出 `[SENSOR]` 日志。
7. Board B should print `[MONITOR] rx` logs and refresh OLED. / 板 B 应输出 `[MONITOR] rx` 日志并刷新 OLED。
