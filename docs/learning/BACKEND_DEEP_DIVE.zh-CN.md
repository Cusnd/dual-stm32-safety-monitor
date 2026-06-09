# 后端与嵌入式深度讲解

[返回 README](../../README.zh-CN.md) | [接线说明](../../WIRING.md) | [硬件资料](../hardware/index.zh-CN.md)

本文面向 C++ 和 STM32 HAL 零基础读者，目标是把本项目后端讲到可以独立解释“为什么这样连线、为什么这样写代码、数据怎样从传感器变成告警和网页数据”。本文只覆盖项目代码、项目定制的 CubeMX 框架代码、构建脚本和测试；`Drivers/CMSIS` 与 `Drivers/STM32F1xx_HAL_Driver` 是第三方库，只说明本项目如何调用，不逐行展开。

## 1. 项目后端一句话

本项目用同一套固件源码编译出两个角色：

| 角色 | 编译宏 | 烧录对象 | 职责 |
|---|---:|---|---|
| SENSOR | `APP_NODE_ROLE=1` | 板 A | 读取 DHT11、MQ135、MQ2、雨量、热敏、火焰，把结果打包为 `22 字节 v2` 二进制帧。 |
| MONITOR | `APP_NODE_ROLE=2` | 板 B | 从 USART3 接收帧，流式解码，判断告警，刷新 OLED，驱动蜂鸣器，记录 W25Q64，并通过 USART1 输出 JSON Lines。 |

最重要的数据链路是：

```text
传感器模块 -> SensorNode -> SensorFrame -> FrameCodec
          -> USART3 -> FrameStreamDecoder -> MonitorNode
          -> AlarmEvaluator -> OLED / Buzzer / W25Q64 / USART1 JSON
```

请始终记住：`PB12/PB13` 在两个角色里含义不同。SENSOR 使用 `PB12` 做 DHT11、`PB13` 做火焰输入；MONITOR 使用 `PB12..PB15` 做 W25Q64 SPI2。这不是一块板同时复用，而是同一仓库按角色编译成两块板的不同固件。

## 2. C++ 零基础读法

| 写法 | 在本项目中的意义 |
|---|---|
| `.hpp` | 声明“别人能调用什么”，例如类、结构体、函数签名。 |
| `.cpp` | 实现“具体怎么做”。 |
| `namespace app` | 把项目自己的名字放进 `app` 命名空间，避免和 HAL 名字冲突。 |
| `struct` | 只装数据的轻量对象，例如 `SensorFrame`、`AlarmThresholds`。 |
| `class` | 数据和操作放在一起，例如 `SensorNode`、`MonitorNode`、`W25q64FlashLogger`。 |
| `enum class` | 有类型保护的枚举，例如 `AlarmState::Danger`，比裸数字更清楚。 |
| `constexpr` | 编译期常量或可在编译期计算的函数，适合阈值、周期、固定表。 |
| `uint8_t` / `uint16_t` / `uint32_t` | 固定位宽整数，嵌入式协议必须精确知道每个字段占几字节。 |

阅读顺序建议：

1. `Core/Src/main.cpp`
2. `App/Config.hpp`
3. `App/Protocol/SensorFrame.hpp`
4. `App/SensorNode.cpp`
5. `App/Protocol/FrameCodec.cpp`
6. `App/Protocol/FrameStreamDecoder.cpp`
7. `App/MonitorNode.cpp`
8. `App/Monitor/AlarmEvaluator.cpp`
9. `App/Drivers/*`
10. `tests/*`

## 3. 构建与生成代码边界

| 文件 | 分类 | 需要掌握的重点 |
|---|---|---|
| `CMakeLists.txt` | 用户维护 | 设置 C/C++ 标准，按 `APP_NODE_ROLE` 选择 SENSOR 或 MONITOR 源文件，生成 ELF/HEX/BIN，提供 ST-LINK/OpenOCD 目标。 |
| `CMakePresets.json` | 用户维护 | `SensorDebug`、`MonitorDebug`、Release 和 ST-LINK helper 的入口。 |
| `cmake/stm32cubemx/CMakeLists.txt` | CubeMX/项目桥接 | 把 Core、HAL、CMSIS、启动文件、链接脚本接入主 target。 |
| `cmake/gcc-arm-none-eabi.cmake` | 工具链 | 使用 ARM GCC 编译 Cortex-M3 固件。 |
| `cmake/starm-clang.cmake` | 工具链 | 另一套可选 ARM 编译器配置。 |
| `cmake/stlink-stm32f103c8.cfg` | 调试烧录 | OpenOCD 连接 ST-LINK 和 STM32F103C8T6 的配置。 |
| `Env-Monitor.ioc` | CubeMX 配置快照 | 可作为外设来源参考，但它和当前手写配置不完全一致，不能覆盖真实源码口径。 |

重要差异：

- 当前代码实际在 `SensorNode::initGpio()` 手动把 `PA4..PA7` 配为模拟输入，并在 `hal::initAdc1()` 直接配置 ADC1 寄存器。
- `.ioc` 中仍可能保留历史演示资源，例如 `PA1..PA3` legacy LED 信息，不能当作当前现役输出。
- `tests/*.cpp` 是桌面 C++ 单元测试风格文件，但顶层 CMake 当前没有明确把它们接成 `ctest` 目标；测试说明应写成“可单独编译运行的协议/解码测试源”，而不是声称已经自动纳入固件构建。

## 4. 启动链路与 Core 代码

### `Core/Src/main.cpp`

| 函数/符号 | 作用 | 调用关系与易错点 |
|---|---|---|
| `APP_ROLE_SENSOR` / `APP_ROLE_MONITOR` | 本地角色常量。 | CMake 把 `APP_NODE_ROLE` 定义成 `1` 或 `2`。 |
| `SystemClock_Config()` | 配置 STM32F103 的 72 MHz 系统时钟。 | 依赖 HSE 和 PLL。若晶振不匹配，串口波特率和延时都会异常。 |
| `main()` | 固件入口。 | `HAL_Init()` -> `SystemClock_Config()` -> GPIO/DWT/USART 初始化 -> 根据角色创建 `SensorNode` 或 `MonitorNode`。 |
| `Error_Handler()` | HAL 初始化失败兜底。 | 常见处理是关中断后死循环；真实调试时可在这里打断点。 |
| `__io_putchar(int ch)` | 把 `printf` 重定向到 USART1。 | 最终调用 `hal::writeDebugChar()`，所以日志和 JSON Lines 从板 B 的 USART1 发出。 |

### `Core/Src/gpio.c` / `Core/Inc/gpio.h`

| 函数 | 作用 |
|---|---|
| `MX_GPIO_Init()` | CubeMX 风格 GPIO 初始化，主要保留板级按键等基础输入配置。项目应用层会继续在 `SensorNode`、`BoardIo`、`W25q64FlashLogger` 中配置自己的引脚。 |

### `Core/Src/spi.c` / `Core/Inc/spi.h`

| 函数/变量 | 作用 |
|---|---|
| `SPI_HandleTypeDef hspi2` | HAL SPI2 句柄，W25Q64 读写使用它。 |
| `MX_SPI2_Init()` | 配置 SPI2 模式、波特率、数据位和主机模式。由 `W25q64FlashLogger::init()` 调用。 |

### `Core/Src/stm32f1xx_hal_msp.c`

| 函数 | 作用 |
|---|---|
| `HAL_MspInit()` | HAL 全局 MSP 初始化，通常打开 AFIO、PWR 等基础时钟。 |
| `HAL_SPI_MspInit(SPI_HandleTypeDef *hspi)` | 当 `hspi2` 初始化时打开 SPI2 和 GPIOB 时钟，配置 `PB13/PB14/PB15` 为 SCK/MISO/MOSI。 |
| `HAL_SPI_MspDeInit(SPI_HandleTypeDef *hspi)` | 释放 SPI2 外设和 GPIO，项目运行态通常不会主动调用。 |

### 自动生成/运行时支撑文件

| 文件 | 作用 | 是否逐行学习 |
|---|---|---|
| `startup_stm32f103xb.s` | 启动汇编，建立中断向量表并跳转到 C 运行时。 | 了解入口即可。 |
| `STM32F103XX_FLASH.ld` | 链接脚本，规定 Flash/RAM 地址、栈、堆和段布局。 | 了解存储布局即可。 |
| `Core/Src/system_stm32f1xx.c` | CMSIS 系统时钟变量和 `SystemInit()`。 | 了解 `SystemCoreClock` 来源即可。 |
| `Core/Src/stm32f1xx_it.c` | 中断处理函数。 | 重点是 USART3 IRQ 应转给 `hal::handleUsart3Irq()`；其他异常 handler 用于调试。 |
| `Core/Src/syscalls.c` / `sysmem.c` | newlib 系统调用桩和堆扩展。 | 了解 `printf`、内存分配如何适配裸机即可。 |

## 5. 全局配置：`App/Config.hpp`

| 类型/常量/函数 | 意义 | 易错点 |
|---|---|---|
| `NodeRole` | 描述 SENSOR/MONITOR 角色。 | 真正选择由 CMake 宏完成，不是运行时菜单。 |
| `AlarmState` | `Normal/Warn/Danger/Waiting/Lost` 五类状态。 | `Waiting` 是还没收到首帧；`Lost` 是曾收到但超时。 |
| `usart_baudrate` | 串口固定 `115200`。 | USART1 和 USART3 都用这个值，但 APB 时钟不同。 |
| `sensor_period_ms` | 板 A 每秒采样并发送。 | DHT11 周期另有 `dht11_period_ms`，不会每秒都重新读。 |
| `ui_period_ms` | OLED 刷新周期。 | 太短会影响软件 I2C 和主循环响应。 |
| `alarm_period_ms` | 蜂鸣器节奏更新周期。 | 危险态实际由 `now / 150` 控制快闪。 |
| `node_timeout_ms` | 板 B 判定节点离线的超时。 | 当前为 5 秒。 |
| `mute_time_ms` | K2 短按静音窗口。 | 静音只影响蜂鸣器，不改变告警状态。 |
| `flash_log_period_ms` | 状态不变时周期性写 Flash 的间隔。 | 状态变化时可提前记录。 |
| `threshold_key_debounce_ms` | 外接阈值按键防抖时间。 | 针对 PB0/PB1 低有效按键。 |
| `AlarmThresholds` | 实际阈值集合。 | 包含 MQ135、MQ2、雨量、热敏预警/危险。 |
| `ThresholdSensor` | 当前正在调节的传感器。 | `Air=MQ135`，`Smoke=MQ2`。 |
| `ThresholdLevels` | 四个传感器各自的 0..4 档位。 | OLED 显示为 1/5..5/5。 |
| `normalizeThresholdLevel()` | 防止越界档位访问数组。 | 文档和前端都应说明越界回默认档。 |
| `thresholdsFromLevels()` | 把档位转换成实际阈值。 | MONITOR、OLED、JSON 都依赖它。 |
| `compatibleThresholdProfile()` | 为旧前端/旧日志保留 `thresholdProfile`。 | 混合档位返回 `255`。 |
| `threshold_profiles[]` | legacy 三档整体 profile。 | 新逻辑优先逐传感器档位。 |

## 6. 引脚定义：`App/BoardPins.hpp`

| 常量 | 节点 | 硬件含义 |
|---|---|---|
| `dht11_port/pin` | SENSOR | DHT11 DATA，`PB12`。 |
| `flame_port/pin` | SENSOR | 火焰 DO，`PB13`，低电平触发。 |
| `therm_do_port/pin` | SENSOR | 热敏 DO，`PB9`，低电平高温触发。 |
| `oled_port/oled_scl_pin/oled_sda_pin` | MONITOR | SSD1306 软件 I2C，`PB6/PB7`。 |
| `buzzer_port/pin` | MONITOR | 有源蜂鸣器，`PB8`，高电平响。 |
| `threshold_select_port/pin` | MONITOR | 外接阈值选择键，`PB0`，低有效。 |
| `threshold_level_port/pin` | MONITOR | 外接档位键，`PB1`，低有效。 |
| `flash_cs_port/pin` | MONITOR | W25Q64 CS，`PB12`。 |

## 7. 协议层

### `App/Protocol/SensorFrame.hpp`

| 字段/枚举 | 意义 |
|---|---|
| `SensorFrame::temp` / `humi` | DHT11 温度和湿度整数值。 |
| `mq135_adc` / `mq2_adc` | MQ135/MQ2 的 12 位 ADC 原始值。 |
| `rain_adc` | 雨量模块 ADC 原始值。 |
| `therm_adc` | 热敏 AO ADC 原始值。 |
| `therm_c10` | 热敏换算温度，单位 0.1 摄氏度。 |
| `flame` | 火焰触发位，`1` 表示触发。 |
| `rain_wet` | 雨量湿态位。 |
| `therm_hot` | 热敏 DO 高温位。 |
| `seq` | 循环帧序号，用于观察丢帧。 |
| `status` | 状态位集合。 |
| `SensorStatus::DhtError` | DHT11 读取失败。 |
| `SensorStatus::ThermHotDigital` | 热敏 DO 高温触发。 |
| `SensorStatus::RainWet` | 雨量湿态触发。 |
| `SensorStatus::ThermAdcError` | 热敏 ADC 接近 0 或满量程，认为异常。 |
| `sensor_status()` | 把 `SensorStatus` 转成 `uint8_t` 方便按位或。 |

### `App/Protocol/FrameCodec.hpp/.cpp`

二进制帧固定为 `22 字节 v2`：

```text
AA 55 LEN VER TEMP HUMI MQ135 MQ2 RAIN THERM_ADC THERM_C10 FLAME RAIN_WET THERM_HOT SEQ STATUS CHECKSUM
```

| 函数/常量 | 作用 | 解释重点 |
|---|---|---|
| `FrameCodec::head0/head1` | 帧头 `AA 55`。 | 解码器靠它从噪声中找到帧起点。 |
| `payload_len` | 负载长度，当前为 `18`。 | 总长度是帧头、长度、版本、负载和校验之和。 |
| `version` | 协议版本，当前为 `2`。 | 前端 JSON schema 也随 v2 扩展字段。 |
| `total_len` | 固定总长度 `22`。 | 所有测试和 PPT 均应使用这个口径。 |
| `FrameOffset` | 每个字段在字节数组里的偏移。 | 私有枚举，避免写魔法数字。 |
| `readU16()` | 从高字节在前的两个字节读 `uint16_t`。 | 串口协议明确采用大端字段。 |
| `writeU16()` | 把 `uint16_t` 写成两个字节。 | `therm_c10` 先转 `uint16_t` 再写入。 |
| `checksum()` | 从 `LEN` 到 payload 末尾逐字节累加取低 8 位。 | 不是 CRC，只用于发现常见串口错误。 |
| `encode()` | 把 `SensorFrame` 打成 22 字节。 | SENSOR 每秒调用一次。 |
| `decode()` | 检查帧头、长度、版本、校验，再恢复结构体。 | MONITOR 的流式解码器调用它。 |

### `App/Protocol/FrameStreamDecoder.hpp/.cpp`

| 函数/类型 | 作用 | 易错点 |
|---|---|---|
| `Result` | `NeedMore/FrameReady/BadFrame` 三种结果。 | 主循环必须只在 `FrameReady` 时更新最新帧。 |
| `reset()` | 清空当前位置。 | MONITOR 初始化时调用。 |
| `push(uint8_t byte, SensorFrame &frame)` | 每喂入一个字节推进一次状态。 | 遇到重叠帧头时保留第二个 `AA`，因此能更快重新同步。 |

## 8. 板 A：`SensorNode`

### `App/SensorNode.hpp`

| 成员 | 作用 |
|---|---|
| `Dht11 dht_` | DHT11 单总线驱动对象。 |
| `last_sensor_ms_` / `last_dht_ms_` | 控制采样周期和 DHT11 周期。 |
| `seq_` | 帧序号。 |
| `temp_` / `humi_` / `dht_ok_` | 最近一次 DHT11 结果。 |
| `avg_valid_` | 标记模拟量滤波器是否已初始化。 |
| `mq135_avg_` / `mq2_avg_` / `rain_avg_` / `therm_avg_` | 四路 ADC 平滑值。 |

### `App/SensorNode.cpp`

| 函数/类型 | 作用 | 调用关系与硬件意义 |
|---|---|---|
| `NtcTablePoint` | NTC 查表点，存 ADC 和 0.1 摄氏度。 | 只在本文件匿名命名空间内使用。 |
| `ntc_table[]` | 10K NTC B3950 参考表。 | ADC 越高，表中温度越低，符合分压方向。 |
| `SensorNode::init()` | 初始化 GPIO、ADC、内部状态并打印启动日志。 | `main()` 在 SENSOR 角色调用。 |
| `SensorNode::initGpio()` | 配置 `PA4..PA7` 模拟输入，火焰/热敏 DO 上拉输入，DHT11 GPIO。 | 这是实际引脚口径，高于 `.ioc` 中的历史信息。 |
| `SensorNode::run()` | 永久主循环，每秒采样、滤波、组帧、发送、打印。 | 读 `ADC1_CH4..7`，读 DHT11，设置 `SensorFrame` 和状态位。 |
| `SensorNode::filter()` | 3:1 一阶平滑：`(previous*3+sample)/4`。 | 首个样本直接采用，避免初值把结果拉偏。 |
| `SensorNode::thermistorAdcToC10()` | 检测 ADC 异常并在线性插值表中换算温度。 | ADC <= 8 或 >= 4088 认为接线/量程异常。 |
| `SensorNode::sendFrame()` | 调 `FrameCodec::encode()` 后用 USART3 发出。 | 最终调用 `hal::sendUsartBuffer(USART3, ...)`。 |

板 A 的重点不是“每个传感器都很准”，而是把多种输入稳定地编码为同一结构。MQ 系列仍是原始 ADC 值，若要换算 ppm，需要现场标定。

## 9. 底层硬件适配：`App/Hal/Hardware`

### `App/Hal/Hardware.hpp`

| 类型/函数 | 作用 |
|---|---|
| `node_rx_buf_size` | USART3 接收环形缓冲区大小，当前 128。 |
| `GpioPin` | 对 HAL GPIO 的轻量封装。 |
| `RxRingBuffer` | USART3 中断接收缓冲。 |
| `initDwtDelay()` / `delayUs()` | 启用 DWT cycle counter，提供微秒延时。 |
| `initDebugUsart1()` | 配置 USART1 调试/JSON 口。 |
| `initNodeUsart3(bool)` | 配置板间 USART3，可选接收中断。 |
| `sendUsartByte()` / `sendUsartBuffer()` | 阻塞发送串口字节。 |
| `readUsartByte()` | 从 USART3 环形缓冲或普通 USART 读字节。 |
| `nodeUsartOverflowCount()` | 查看 USART3 缓冲溢出次数。 |
| `writeDebugChar()` | 支持 `printf` 输出。 |
| `handleUsart3Irq()` | USART3 IRQ 中读 DR 并入队。 |
| `initAdc1()` / `readAdc1Channel()` | 直接寄存器方式配置和读取 ADC1。 |

### `App/Hal/Hardware.cpp`

| 函数 | 解释重点 |
|---|---|
| `GpioPin::write()` | 调 `HAL_GPIO_WritePin()` 输出高/低。 |
| `GpioPin::toggle()` | 翻转输出。 |
| `GpioPin::isSet()` / `isReset()` | 读取输入或输出状态。 |
| `RxRingBuffer::clear()` | 复位 `head/tail/overflow_count`。 |
| `RxRingBuffer::pushFromIsr()` | 中断上下文写入，满了就增加溢出计数并丢弃新字节。 |
| `RxRingBuffer::read()` | 主循环读取一个字节，没有数据返回 `-1`。 |
| `RxRingBuffer::overflowCount()` | 调试接收压力。 |
| `initDwtDelay()` | 直接设置 `CoreDebug` 和 `DWT` 寄存器。 |
| `delayUs()` | 忙等微秒延时，DHT11 和软件 I2C 依赖它。 |
| `initDebugUsart1()` | 用 HAL 配 GPIO，用寄存器设置 USART1 BRR/CR1。 |
| `initNodeUsart3()` | 用 HAL 配 `PB10/PB11`，用寄存器设置 USART3；MONITOR 启用 RXNE 中断。 |
| `sendUsartByte()` | 等 `TXE` 后写 `DR`，简单可靠但阻塞。 |
| `sendUsartBuffer()` | 逐字节发送。 |
| `readUsartByte()` | USART3 走环形缓冲，其他 USART 直接看 `RXNE`。 |
| `writeDebugChar()` | 把 `\n` 前补 `\r`，兼容串口终端换行。 |
| `handleUsart3Irq()` | 处理 `RXNE` 或 `ORE`，读 `DR` 同时清相关标志。 |
| `initAdc1()` | 设置 ADC 分频、采样时间、校准流程。 |
| `readAdc1Channel()` | 单通道阻塞转换，返回 12 位值。 |

注意：USART 和 ADC 不是完全由 HAL 初始化。项目选择“HAL 配 GPIO + 寄存器配核心外设”的混合写法，文档和讲解都应如实说明。

## 10. 传感器与板级 I/O 驱动

### `App/Drivers/Dht11.hpp/.cpp`

文件拆分：

| 文件 | 负责内容 |
|---|---|
| `App/Drivers/Dht11.hpp` | 声明 DHT11 驱动类、公开读取接口和私有时序辅助函数。 |
| `App/Drivers/Dht11.cpp` | 实现 GPIO 方向切换、微秒等待、40 bit 数据读取和校验。 |

| 函数 | 作用 |
|---|---|
| `Dht11::initGpio()` | 配置 DATA 引脚初始状态。 |
| `Dht11::setOutput()` | 把 DATA 配为开漏输出，用于主机拉低启动信号。 |
| `Dht11::setInput()` | 把 DATA 切回输入，上拉等待传感器响应。 |
| `Dht11::waitLevel(int level, uint32_t timeout_us)` | 等待引脚达到指定电平，带超时。 |
| `Dht11::read(uint8_t &temp, uint8_t &humi)` | 完整执行 DHT11 40 bit 时序，校验 checksum，更新温湿度。 |

DHT11 的关键是微秒级时序，所以依赖 `hal::delayUs()`。若读数经常失败，先查 DATA 上拉、电源、线长、采样间隔。

### `App/Drivers/BoardIo.hpp/.cpp`

文件拆分：

| 文件 | 负责内容 |
|---|---|
| `App/Drivers/BoardIo.hpp` | 声明 `Buzzer`、`Buttons` 以及按键/蜂鸣器对外接口。 |
| `App/Drivers/BoardIo.cpp` | 实现 PB8 蜂鸣器、板载 K1/K2、外接阈值键 PB0/PB1 的 GPIO 配置和读取。 |

| 类/函数 | 作用 |
|---|---|
| `Buzzer::init()` | 配置 `PB8` 推挽输出，默认关闭。 |
| `Buzzer::set(bool on)` | 高电平打开有源蜂鸣器。 |
| `Buttons::init()` | 配置外接阈值按键 `PB0/PB1` 为内部上拉输入。 |
| `Buttons::key1Pressed()` | 读取板载 K1，参考板高电平按下。 |
| `Buttons::key2Pressed()` | 读取板载 K2，参考板高电平按下。 |
| `Buttons::thresholdSelectPressed()` | 读取 PB0，低电平表示按下。 |
| `Buttons::thresholdLevelPressed()` | 读取 PB1，低电平表示按下。 |

这里最容易错的是极性：K1/K2 是高有效，外接阈值键是低有效。

### `App/Drivers/OledDisplay.hpp/.cpp`

文件拆分：

| 文件 | 负责内容 |
|---|---|
| `App/Drivers/OledDisplay.hpp` | 声明 SSD1306 显示类、初始化、清屏、定位和打印接口。 |
| `App/Drivers/OledDisplay.cpp` | 实现 5x7 字库、软件 I2C 时序、SSD1306 初始化序列和行打印。 |

| 函数/类型 | 作用 |
|---|---|
| `font5x7()` | 把 ASCII 字符转成 5 列字模。 |
| `OledDisplay::initBus()` | 配置 `PB6/PB7` 为软件 I2C 所需 GPIO。 |
| `delay()` | 软件 I2C 小延时。 |
| `sda()` / `scl()` | 拉高/拉低 SDA 和 SCL。 |
| `start()` / `stop()` | I2C 起始/停止条件。 |
| `writeByte()` | 按位发送 1 字节并处理 ACK 时钟。 |
| `write(control, data)` | 向 SSD1306 写命令或数据。 |
| `cmd()` | 写一个控制命令。 |
| `dataFill()` | 连续填充显示数据。 |
| `dataBuffer()` | 写一段字模 buffer。 |
| `initController()` | 发送 SSD1306 初始化序列。 |
| `clear()` | 清屏。 |
| `setCursor(page, col)` | 设置页地址和列地址。 |
| `printLine(page, text)` | 在指定页打印一行文本，不足处清空。 |

OLED 驱动是项目自写软件 I2C，不依赖硬件 I2C 外设。优点是引脚灵活，缺点是刷新时占用 CPU。

## 11. 板 B 监控节点

### `App/Monitor/AlarmEvaluator.hpp/.cpp`

| 类型/函数 | 作用 |
|---|---|
| `AlarmEvaluation` | 保存 `waiting/lost/danger/warn/muted/state`。 |
| `isMuted()` | 判断当前 tick 是否仍在静音窗口内。 |
| `hasThermAdc()` | 热敏 ADC 未报错才允许参与温度阈值判断。 |
| `hasDangerCondition()` | 火焰、MQ2 危险、热敏 DO、热敏危险温度。 |
| `hasWarnCondition()` | DHT 错误、MQ135、MQ2 预警、雨量、热敏预警。 |
| `evaluateAlarm()` | 合成最终状态，优先级为 Danger > Waiting > Lost > Warn > Normal。 |
| `alarmStateString()` | 把状态转成 JSON 字符串。 |

静音不改变 `state`，只影响蜂鸣器输出。网页和 OLED 仍应显示真实风险。

### `App/Monitor/DisplayFormatter.hpp/.cpp`

| 函数/类型 | 作用 |
|---|---|
| `MonitorDisplayLines` | 四行 OLED 文本缓冲，每行固定长度。 |
| `stateTitle()` | 把告警状态转成首页标题。 |
| `tempFraction()` | 处理负数小数位显示。 |
| `thresholdSensorName()` | 把当前调节项转成 `MQ135/MQ2/RAIN/THERM`。 |
| `thresholdLevelForSensor()` | 读取某传感器当前档位。 |
| `formatSelectedThresholdLine()` | 根据当前传感器生成阈值说明行。 |
| `formatMonitorDisplay()` | 生成 OLED 两页内容：实时页和阈值/日志页。 |

该文件把“显示格式”从 `MonitorNode` 主循环拆出来，便于测试和修改。

### `App/MonitorNode.hpp/.cpp`

| 函数/成员 | 作用 | 调用关系与易错点 |
|---|---|---|
| `MonitorNode::init()` | 复位状态，初始化蜂鸣器、按键、OLED、Flash，显示等待页面。 | `main()` 在 MONITOR 角色调用。 |
| `MonitorNode::run()` | 永久主循环，分时处理接收、按键、Flash、告警、显示和日志。 | 这是协作式调度，不是 RTOS。 |
| `processRx(uint32_t now)` | 从 USART3 环形缓冲取字节，喂给 `FrameStreamDecoder`。 | 收到完整帧后更新 `latest_frame_` 并输出 JSON。 |
| `updateButtons(uint32_t now)` | 处理 K1/K2/PB0/PB1。 | K2 短按静音 60 秒；PB0/PB1 调整阈值。 |
| `pressedEdge()` | 对低有效外接阈值键做防抖和上升边沿生成。 | 只在稳定按下时返回一次。 |
| `selectedThresholdLevel()` 非 const | 返回当前传感器档位引用，允许修改。 | `level_pressed` 分支用它递增。 |
| `selectedThresholdLevel() const` | 返回归一化后的当前档位。 | 打印和显示使用。 |
| `selectedThresholdName()` | 返回当前传感器名字。 | 日志和 OLED 使用。 |
| `printFrontendJson()` | 把最新帧和告警结果输出为 JSON Lines schema v2。 | 前端 parser 要求 v2 扩展字段齐全。 |
| `updateAlarm()` | 根据状态和静音窗口控制蜂鸣器节奏。 | Danger 快速响，Lost 慢速响，Warn 不响。 |
| `updateDisplay()` | 调 `formatMonitorDisplay()` 并写四行 OLED。 | 页面由 K1 或阈值键切换。 |

`MonitorNode::run()` 每轮都先 `flash_.process()`，因此 Flash 擦写是状态机推进，不会在主循环里长时间阻塞等待。

## 12. W25Q64 Flash 记录器

### `App/Drivers/W25q64FlashLogger.hpp`

| 成员/常量/函数 | 作用 |
|---|---|
| `flash_size_bytes` | W25Q64 总容量 8 MB。 |
| `sector_size` | 扇区大小 4096 字节。 |
| `meta_entry_size` | 元数据记录 16 字节。 |
| `record_magic` | 日志记录魔数。 |
| `log_start_addr` | 从 sector 1 开始写日志，sector 0 留给元数据。 |
| `log_record_size` | 每条日志 32 字节。 |
| `Task` | 非阻塞任务状态：擦日志扇区、写日志、擦元数据、写元数据。 |
| `init()` | 初始化 SPI2 和 CS，读 JEDEC ID，恢复游标。 |
| `logFrame()` | 构造待写入 32 字节记录。 |
| `process()` | 推进擦除/写入/元数据任务。 |
| `present()` | Flash 是否可用。 |
| `busy()` | 是否有未完成任务。 |
| `recordCount()` | 累计记录数。 |

### `App/Drivers/W25q64FlashLogger.cpp`

| 函数 | 作用 | 易错点 |
|---|---|---|
| `packThresholdLevels()` | 把四个阈值档位和静音位打包进 16 bit。 | 每个档位用 3 bit，静音用 bit12。 |
| `init()` | 初始化 GPIO/SPI，读 `0x9F` JEDEC ID。 | 只接受常见 W25Q64 ID；失败时 `present_=0`，系统仍运行。 |
| `logFrame()` | 填充 `pending_record_`，包括帧、状态、tick、阈值、计数、CRC。 | 如果已有待写任务，直接返回 false。 |
| `process()` | 若任务忙则轮询 WIP 位；若空闲则启动待写日志或元数据。 | 主循环频繁调用，避免一次性阻塞。 |
| `present()` / `busy()` / `recordCount()` | 对外状态接口。 | OLED 和 JSON 使用 `present/recordCount`。 |
| `cs()` | 控制片选。 | SPI 事务必须成对拉低/拉高。 |
| `txRx()` | HAL SPI 同步收发 1 字节。 | 失败时返回 `0xFF`。 |
| `readStatus()` | 发送 `0x05` 读取状态寄存器。 | WIP bit0 表示忙。 |
| `flashBusy()` | 检查 WIP。 | 超时会禁用记录器。 |
| `writeEnable()` | 发送 `0x06`。 | 擦除/写入前必须调用。 |
| `startSectorErase()` | 发送 `0x20 + 24bit addr`，设置当前任务。 | 扇区擦除超时长于页写。 |
| `startPageProgram()` | 发送 `0x02 + addr + data`。 | 当前记录 32 字节，不跨页。 |
| `completeTask()` | 根据任务类型更新游标、计数、元数据地址。 | 写完日志后立即 `scheduleMetadata()`。 |
| `failTask()` | 超时后取消待写并禁用 Flash 功能。 | 不影响报警、OLED、JSON。 |
| `readData()` | 发送 `0x03` 顺序读。 | `loadMetadata()` 用它扫描 sector 0。 |
| `crc16()` | Modbus 风格 CRC16。 | 用于日志和元数据完整性。 |
| `u32ToBytes()` / `bytesToU32()` | 大端 32 bit 转换。 | 与记录格式一致。 |
| `loadMetadata()` | 扫描元数据扇区，恢复最后有效游标。 | 空白位置是下一次写入地址。 |
| `scheduleMetadata()` | 生成一条待写元数据。 | 由初始化失败重建或日志完成后调用。 |

W25Q64 是可选历史记录能力，不是完整磨损均衡数据库。它采用固定记录、环形地址和 sector 0 游标，足以支撑项目展示和基础追溯。

## 13. 测试代码

### `tests/protocol_frame_codec_test.cpp`

| 函数 | 作用 |
|---|---|
| `assertFrameEqual()` | 比较两个 `SensorFrame` 所有字段。 |
| `sampleFrame()` | 构造包含温湿度、MQ、雨量、热敏、火焰、状态位的样例帧。 |
| `roundTrip()` | 验证 encode 后 decode 能恢复原始结构。 |
| `rejectsCorruptFrames()` | 验证坏帧头、坏长度、坏版本、坏校验都会被拒绝。 |
| `main()` | 顺序运行测试，成功返回 0。 |

### `tests/frame_stream_decoder_test.cpp`

| 函数 | 作用 |
|---|---|
| `makeFrame()` | 构造不同序号的样例帧。 |
| `assertSameFrame()` | 比较解码结果。 |
| `decodesAfterNoise()` | 验证噪声后能找到帧。 |
| `resyncsOverlappingHeaders()` | 验证 `AA AA 55...` 这类重叠帧头能恢复。 |
| `rejectsBadFrameThenRecovers()` | 坏帧后下一帧仍能接收。 |
| `decodesConsecutiveFrames()` | 连续多帧逐帧输出。 |
| `main()` | 顺序运行测试。 |

这两个测试主要证明协议边界和流式恢复能力，不证明真实串口、电源和传感器标定。

## 14. 硬件资料引用

项目内优先引用：

- `WIRING.md`：当前接线和电气限制。
- `docs/hardware/index.zh-CN.md`：芯片和模块入口。
- `docs/hardware/modules/*.zh-CN.md`：模块原理、引脚和本项目映射。
- `docs/hardware/chips/*.zh-CN.md`：关键芯片背景。

本地 Wildfire 资料库用于原理图和课程参考：

```text
C:\baidunetdiskdownload\野火小智STM32F103C8_HAL库实战资料与课件
```

建议查找位置：

| 资料 | 位置 |
|---|---|
| 核心板原理图、尺寸、IO 表 | `1-硬件资料\原理图_尺寸图_规格书_IO表格_封装库` |
| HAL/参考手册/数据手册 | `3-参考资料` |
| USART、ADC、DHT11、OLED、W25Q64 例程 | `2-配套程序` |
| DHT11、MQ135、MQ2、雨量、热敏、火焰、OLED、蜂鸣器、W25Q64 模块资料 | `5-配套模块` |

## 15. 高频追问口径

| 问题 | 建议回答 |
|---|---|
| 为什么用两块板？ | 采集和监测职责分离，板 A 负责现场采样，板 B 负责接收、告警、显示、记录和对浏览器输出，故障定位更清楚。 |
| 为什么 USART3 做板间通信？ | USART1 留给调试和 JSON Lines；USART3 用 `PB10/PB11` 做板间二进制链路，线序清楚，互不抢占。 |
| 为什么 TX 接对方 RX？ | 串口是发送端 TX 到接收端 RX；两板还必须共地，否则电平没有共同参考。 |
| 为什么要帧头和校验？ | 帧头帮助从字节流中找到边界，校验帮助拒绝损坏帧；流式解码器能在噪声后重新同步。 |
| 为什么 MQ 不换算 ppm？ | 需要传感器预热、标定曲线和现场校准。当前项目保留 ADC 原始值，更适合展示阈值、趋势和告警逻辑。 |
| 为什么 OLED 用软件 I2C？ | 项目只需低速文本显示，软件 I2C 占用外设少，引脚灵活。 |
| Flash 不接会怎样？ | W25Q64 是可选记录能力，不接时监测、报警、OLED 和 JSON 输出仍工作。 |
| `.ioc` 和代码不一致怎么办？ | 以当前源码、`WIRING.md`、`BoardPins.hpp` 为准；`.ioc` 是配置快照，不是唯一事实来源。 |

## 16. 修改入口速查

| 想修改 | 后端入口 | 同步影响 |
|---|---|---|
| 添加传感器字段 | `SensorFrame`、`FrameCodec`、`SensorNode::run()`、`MonitorNode::printFrontendJson()` | 前端 `types.ts/parser.ts/analysis.ts`、文档、测试。 |
| 调整阈值 | `Config.hpp` 的五档数组 | OLED、JSON、前端分析。 |
| 修改告警优先级 | `AlarmEvaluator.cpp` | 蜂鸣器、OLED、JSON、AI 风险解释。 |
| 修改 OLED 文本 | `DisplayFormatter.cpp` | 需要实机或渲染检查文本长度。 |
| 修改板间协议 | `FrameCodec`、`FrameStreamDecoder`、测试 | 两块板必须同时更新。 |
| 修改 Flash 记录格式 | `W25q64FlashLogger::logFrame()`、CRC、元数据策略 | 旧记录兼容和后续导出工具。 |
| 改引脚 | `BoardPins.hpp`、对应 GPIO 初始化、`WIRING.md` | `.ioc`、硬件图、PPT 也要同步。 |
