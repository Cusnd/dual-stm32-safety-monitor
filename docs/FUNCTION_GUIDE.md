# Function Guide / 函数说明

[English README](../README.md) | [中文 README](../README.zh-CN.md) | [Detailed Design](FUNCTION_DESIGN_WALKTHROUGH.en.md) | [详细设计](FUNCTION_DESIGN_WALKTHROUGH.zh-CN.md) | [Project Structure / 项目结构](PROJECT_STRUCTURE.md)

This guide explains the important project functions for beginners.  
这份文档面向初学者，解释项目里主要函数的作用、调用关系和阅读顺序。

For a deeper explanation of how these functions coordinate as a complete system, see [FUNCTION_DESIGN_WALKTHROUGH.en.md](FUNCTION_DESIGN_WALKTHROUGH.en.md).

如果想看更细的函数设计理由、协作逻辑和图表串联，请继续阅读 [FUNCTION_DESIGN_WALKTHROUGH.zh-CN.md](FUNCTION_DESIGN_WALKTHROUGH.zh-CN.md)。

## How To Read The Firmware / 如何阅读固件

Start from `main()` in [Core/Src/main.cpp](../Core/Src/main.cpp). The firmware is compiled into two different programs by `APP_NODE_ROLE`:

先从 [Core/Src/main.cpp](../Core/Src/main.cpp) 里的 `main()` 看起。项目通过 `APP_NODE_ROLE` 把同一份源码编译成两个不同程序：

| Role / 角色 | Build preset / 构建预设 | Main loop / 主循环 | Board / 板子 |
|---|---|---|---|
| Sensor node / 采集节点 | `SensorDebug` | `SensorNode::run()` | Board A / 板 A |
| Monitor node / 显示报警节点 | `MonitorDebug` | `MonitorNode::run()` | Board B / 板 B |

The most useful reading order is:

推荐阅读顺序：

1. `main()` and role `init()` methods / 先看入口和节点初始化。
2. `SensorNode::run()` / 再看板 A 如何采样并发送数据。
3. `MonitorNode::run()` / 再看板 B 如何接收、显示和报警。
4. `FrameCodec::encode()` and `FrameCodec::decode()` / 最后看串口数据帧如何打包和校验。

## Startup And Role Selection / 启动与角色选择

| Function / 函数 | What it does / 作用 |
|---|---|
| `main()` | Initializes HAL, system clock, GPIO, and application peripherals, then enters the selected role loop. / 初始化 HAL、系统时钟、GPIO 和应用外设，然后进入当前角色的主循环。 |
| `SystemClock_Config()` | Sets 8 MHz HSE x 9 as 72 MHz SYSCLK, with APB1 at 36 MHz. / 配置 8 MHz 外部晶振经 PLL 倍频到 72 MHz，APB1 为 36 MHz。 |
| `SensorNode::init()` / `MonitorNode::init()` | Initializes role-specific peripherals after shared HAL/USART setup. / 通用 HAL/串口初始化后，再初始化对应节点专用外设。 |
| `Error_Handler()` | Disables interrupts and stops in an infinite loop when a fatal initialization error occurs. / 出现严重初始化错误时关闭中断并停在死循环。 |

## Serial Communication / 串口通信

| Function / 函数 | What it does / 作用 |
|---|---|
| `hal::initDebugUsart1()` | Configures USART1 on `PA9/PA10` for `printf` debugging through the target board USB-UART bridge. / 配置 `PA9/PA10` 上的 USART1，通过目标板 USB 转串口打印调试信息。 |
| `hal::initNodeUsart3()` | Configures USART3 on `PB10/PB11` for board-to-board communication. The monitor enables receive interrupt. / 配置 `PB10/PB11` 上的 USART3 做双板通信；显示节点会开启接收中断。 |
| `hal::sendUsartByte()` | Waits until the UART transmit register is empty, then sends one byte. / 等待串口发送寄存器空闲后发送 1 个字节。 |
| `hal::sendUsartBuffer()` | Sends a byte array by repeatedly calling `hal::sendUsartByte()`. / 循环调用 `hal::sendUsartByte()` 发送一段字节数组。 |
| `hal::readUsartByte()` | Reads one byte from USART3 ring buffer or from a polling UART; returns `-1` if no byte is available. / 从 USART3 环形缓冲或轮询串口读取 1 字节；没有数据时返回 `-1`。 |
| `USART3_IRQHandler()` | Interrupt handler that stores received USART3 bytes into a ring buffer. / USART3 中断服务函数，把收到的字节放进环形缓冲。 |
| `__io_putchar()` | Redirects `printf()` output to USART1. / 把 `printf()` 输出重定向到 USART1。 |

## Sensor Node / 采集节点

| Function / 函数 | What it does / 作用 |
|---|---|
| `SensorNode::run()` | Board A super-loop: sends one frame per second, refreshes MQ/flame every frame, refreshes DHT11 at a safe interval, smooths MQ values, and prints debug logs. / 板 A 主循环：每秒发送一帧，每帧刷新 MQ/火焰，按安全间隔刷新 DHT11，平滑 MQ 数值，并打印调试日志。 |
| `SensorNode::init()` | Configures DHT11, flame sensor, MQ135, MQ2, rain, thermistor pins, and ADC1. / 配置 DHT11、火焰、MQ、雨量、热敏引脚以及 ADC1。 |
| `hal::initAdc1()` | Enables and calibrates ADC1 for analog readings. / 使能并校准 ADC1，用于读取模拟量。 |
| `hal::readAdc1Channel()` | Performs one ADC conversion on the selected channel and returns a 12-bit value. / 对指定 ADC 通道采样一次，返回 12 位原始值。 |
| `DHT11_SetOutput()` | Sets the DHT11 data pin as open-drain output so the MCU can send the start signal. / 将 DHT11 数据脚设为开漏输出，用于 MCU 发送起始信号。 |
| `DHT11_SetInput()` | Sets the DHT11 data pin as input so the sensor can drive the bus. / 将 DHT11 数据脚设为输入，让传感器接管总线。 |
| `DHT11_WaitLevel()` | Waits for the DHT11 line to become high or low, with a microsecond timeout. / 等待 DHT11 数据线变为指定电平，带微秒级超时。 |
| `Dht11::read()` | Reads humidity and temperature from DHT11 and verifies its checksum. / 读取 DHT11 温湿度并校验数据和。 |
| `Sensor_SendFrame()` | Encodes one `SensorFrame` and sends it over USART3. / 将一个 `SensorFrame` 编码后通过 USART3 发出。 |

## Frame Protocol / 数据帧协议

| Function / 函数 | What it does / 作用 |
|---|---|
| `Frame_Checksum()` | Adds bytes together and returns the low 8 bits as checksum. / 将字节累加，并取低 8 位作为校验和。 |
| `FrameCodec::encode()` | Converts a `SensorFrame` structure into the 22-byte v2 wire format. / 把 `SensorFrame` 结构体转换成 22 字节 v2 串口帧。 |
| `FrameCodec::decode()` | Checks header, length, checksum, then converts bytes back into `SensorFrame`. / 检查帧头、长度和校验和，再把字节还原成 `SensorFrame`。 |

Frame format:

数据帧格式：

```text
AA 55 LEN VER TEMP HUMI MQ135 MQ2 RAIN THERM THERM_C10 FLAME RAIN_WET THERM_HOT SEQ STATUS CHECKSUM
```

`STATUS` bits: `bit0=DHT error`, `bit1=thermistor DO hot`, `bit2=rain wet`, `bit3=thermistor ADC fault`.

`STATUS` 位定义：`bit0=DHT 错误`，`bit1=热敏 DO 高温`，`bit2=雨量湿态`，`bit3=热敏 ADC 异常`。

## Monitor Node / 显示报警节点

| Function / 函数 | What it does / 作用 |
|---|---|
| `MonitorNode::run()` | Board B super-loop: receives frames, handles buttons, updates alarm, refreshes OLED, and logs to flash if present. / 板 B 主循环：接收数据帧、处理按键、更新报警、刷新 OLED，并在有 Flash 时记录日志。 |
| `Monitor_GPIO_Init()` | Configures buzzer, OLED software-I2C pins, and initial RGB LED state. / 配置蜂鸣器、OLED 软件 I2C 引脚和 RGB 初始状态。 |
| `MonitorNode::processRx()` | Pulls bytes from the ring buffer, searches for `AA 55`, decodes complete frames, and updates latest data. / 从环形缓冲取字节，寻找 `AA 55` 帧头，解码完整数据帧并更新最新数据。 |
| `Monitor_UpdateButtons()` | Detects K1/K2 edges: K1 switches page, K2 short press mutes, K2 long press changes threshold profile. / 检测 K1/K2 边沿：K1 切页，K2 短按静音，K2 长按切换阈值档位。 |
| `Monitor_NodeLost()` | Returns true when no valid sensor frame has arrived for more than 3 seconds. / 超过 3 秒没有收到合法采集帧时返回真。 |
| `Monitor_Danger()` | Checks flame, serious smoke, and thermistor high-temperature conditions. / 判断火焰、烟雾严重超标和热敏高温。 |
| `Monitor_Warn()` | Checks air/smoke, rain wet, thermistor warning, and DHT11 error status. / 判断空气/烟雾、雨量湿态、热敏预警和 DHT11 错误状态。 |
| `Monitor_UpdateAlarm()` | Selects LED color and buzzer pattern according to alarm priority. / 按报警优先级选择 LED 颜色和蜂鸣器节奏。 |
| `MonitorNode::updateDisplay()` | Writes the current page to the OLED. / 将当前页面内容写到 OLED。 |
| `LED_Set()` | Controls the active-low on-board RGB LED. / 控制低电平点亮的板载 RGB LED。 |
| `Buzzer_Set()` | Turns the buzzer on or off. / 打开或关闭蜂鸣器。 |
| `WS2813_Init_Custom()` | Configures TIM3_CH1 + DMA for external RGB output. / 配置 TIM3_CH1 + DMA，用于外置 RGB 输出。 |
| `WS2813_SetColor()` | Sends one GRB color frame to the external WS2813E LED. / 向外置 WS2813E 发送一帧 GRB 颜色数据。 |

## OLED Driver / OLED 驱动

These functions implement a tiny SSD1306-style software-I2C display driver. It is intentionally simple so the project remains easy to inspect, modify, and demonstrate.

这些函数实现了一个小型 SSD1306 风格的软件 I2C OLED 驱动。为了便于阅读、修改和演示，代码刻意保持简单。

| Function / 函数 | What it does / 作用 |
|---|---|
| `I2C_Delay()` | Provides a short delay between software-I2C signal changes. / 在软件 I2C 电平变化之间插入短延时。 |
| `I2C_SDA()` / `I2C_SCL()` | Drive SDA or SCL high/low. / 控制 SDA 或 SCL 高低电平。 |
| `I2C_Start()` / `I2C_Stop()` | Generate I2C start and stop conditions. / 产生 I2C 起始和停止条件。 |
| `I2C_WriteByte()` | Shifts out one byte on the software-I2C bus. / 在软件 I2C 总线上移出 1 个字节。 |
| `OLED_Write()` | Sends one command or data byte to SSD1306 address `0x3C`. / 向地址 `0x3C` 的 SSD1306 发送命令或数据。 |
| `OLED_Cmd()` / `OLED_Data()` | Convenience wrappers for command and display data. / OLED 命令和显示数据的封装函数。 |
| `OLED_Init_Custom()` | Sends the OLED initialization command sequence. / 发送 OLED 初始化命令序列。 |
| `OLED_Clear()` | Clears the whole OLED display. / 清空 OLED 全屏。 |
| `OLED_SetCursor()` | Sets OLED page and column position. / 设置 OLED 页地址和列地址。 |
| `Font5x7()` | Converts ASCII characters into 5x7 font columns. / 将 ASCII 字符转换为 5x7 点阵字模。 |
| `OLED_PutChar()` | Draws one character. / 显示 1 个字符。 |
| `OLED_Puts()` | Draws a string. / 显示一个字符串。 |
| `OLED_PrintLine()` | Prints a string at the start of one OLED page. / 在指定 OLED 页起始位置显示一行字符串。 |

## Optional W25Q64 Flash / 可选 W25Q64 Flash

| Function / 函数 | What it does / 作用 |
|---|---|
| `Flash_Init_Custom()` | Configures SPI2, reads JEDEC ID, and enables logging only when a valid chip is detected. / 配置 SPI2，读取 JEDEC ID，只有检测到有效芯片时才启用日志。 |
| `Flash_CS()` | Controls the flash chip select pin. / 控制 Flash 片选引脚。 |
| `SPI2_TxRx()` | Sends one byte over SPI2 and returns the received byte. / 通过 SPI2 发送 1 字节并返回同时接收的字节。 |
| `Flash_ReadStatus()` | Reads the W25Q status register. / 读取 W25Q 状态寄存器。 |
| `Flash_WaitReady()` | Waits until flash is not busy, with timeout. / 等待 Flash 忙状态结束，带超时保护。 |
| `Flash_WriteEnable()` | Sends the write-enable command required before erase/program. / 发送写使能命令，擦除或写入前必须执行。 |
| `Flash_SectorErase()` | Erases one 4 KB sector. / 擦除一个 4 KB 扇区。 |
| `Flash_PageProgram()` | Programs a short data record into flash. / 向 Flash 写入一段较短记录。 |
| `Flash_ReadData()` | Reads arbitrary bytes from W25Q64. / 从 W25Q64 读取任意字节。 |
| `Flash_LoadMetadata()` | Restores the circular-log cursor from sector 0 metadata. / 从 sector 0 元数据恢复环形日志游标。 |
| `Flash_WriteMetadata()` | Appends the next cursor metadata entry. / 追加写入下一条游标元数据。 |
| `W25q64FlashLogger::logFrame()` | Saves one fixed 32-byte v2 circular-log record. / 写入一条固定 32 字节 v2 环形日志记录。 |

## Common Beginner Questions / 初学者常见问题

**Why are there two firmware images from one source file?**  
Because both boards share the same protocol and many helper functions. `APP_NODE_ROLE` selects which role loop is compiled as the active program.

**为什么一份源码会生成两个固件？**  
因为两块板共用协议和很多工具函数，`APP_NODE_ROLE` 决定当前固件运行采集节点还是显示节点。

**Why not use USART1 for board-to-board communication?**  
USART1 `PA9/PA10` is kept for the target board USB-UART debug bridge, so board-to-board traffic uses USART3 instead.

**为什么不用 USART1 做双板通信？**  
USART1 的 `PA9/PA10` 保留给目标板 USB 转串口调试桥，双板通信因此使用 USART3。

**Where should I add a new sensor?**  
Add its pin definitions in `App/BoardPins.hpp`, initialize the GPIO/ADC in `SensorNode::init()` or a driver class, read it in `SensorNode::run()`, and extend the frame protocol only if the monitor also needs the new value.

**如果要加新传感器，应该改哪里？**  
先在 `App/BoardPins.hpp` 添加引脚定义，再到 `SensorNode::init()` 或对应驱动类初始化，在 `SensorNode::run()` 读取；如果板 B 也需要显示该值，再扩展数据帧协议。
