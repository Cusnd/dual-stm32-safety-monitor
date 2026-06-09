# 函数说明

[中文 README](../README.zh-CN.md) | [English](FUNCTION_GUIDE.en.md) | [详细设计](FUNCTION_DESIGN_WALKTHROUGH.zh-CN.md) | [项目结构](PROJECT_STRUCTURE.zh-CN.md)

这份文档解释当前 C++ 后端里最重要的函数和类。建议先从 `Core/Src/main.cpp` 开始，再按 `APP_NODE_ROLE` 选择的角色继续阅读。

## 阅读顺序

| 步骤 | 代码 | 作用 |
|---|---|---|
| 1 | `main()` | 初始化 HAL、GPIO、计时、调试 USART1 和板间 USART3。 |
| 2 | `SensorNode::run()` | 理解板 A 如何采样传感器，并每秒发送一帧协议 v2 数据。 |
| 3 | `FrameCodec` 和 `FrameStreamDecoder` | 理解二进制帧格式以及板 B 如何在噪声后重新同步。 |
| 4 | `MonitorNode::run()` | 理解板 B 的协作式调度：接收、按键、Flash、报警、显示、记录。 |
| 5 | `AlarmEvaluator` 和 `DisplayFormatter` | 理解报警判断和 OLED 文本格式化如何从主循环拆出。 |

## 启动与角色选择

| 函数或类 | 作用 |
|---|---|
| `main()` | 通过编译期 `APP_NODE_ROLE` 选择 `SensorNode` 或 `MonitorNode`。 |
| `SystemClock_Config()` | 配置 8 MHz HSE x 9 为 72 MHz SYSCLK，APB1 为 36 MHz。 |
| `MX_GPIO_Init()` | 通过 CubeMX 风格生成代码初始化板级 K1/K2 输入。 |
| `hal::initDwtDelay()` | 启用 DWT 微秒延时，供 DHT11 和软件 I2C 使用。 |
| `hal::initDebugUsart1()` | 配置 USART1 `PA9/PA10` 为 `115200 8N1`，用于日志和 JSON Lines。 |
| `hal::initNodeUsart3(enable_rx_interrupt)` | 配置 USART3 `PB10/PB11`；MONITOR 会启用 RX 中断和环形缓冲。 |

## 采集节点

| 函数或类 | 作用 |
|---|---|
| `SensorNode::init()` | 初始化板 A GPIO、ADC1、DHT11 状态、滤波器和序号计数器。 |
| `SensorNode::run()` | 采样 MQ135、MQ2、雨量、热敏 AO/DO、火焰 DO 和 DHT11；填充 `SensorFrame`；发送并打印日志。 |
| `SensorNode::filter()` | 首次有效样本之后，对模拟量应用 3:1 简单平滑滤波。 |
| `SensorNode::thermistorAdcToC10()` | 用本地查表/插值把热敏 ADC 转为 0.1 摄氏度，并报告 ADC 异常。 |
| `SensorNode::sendFrame()` | 调用 `FrameCodec::encode()` 并通过 USART3 发送编码后的帧。 |
| `Dht11::read()` | 执行 DHT11 通信流程，校验 checksum 后更新温湿度。 |
| `hal::initAdc1()` / `hal::readAdc1Channel()` | 配置 ADC1，并执行阻塞式单通道 12 位采样。 |

## 协议与流式解码

| 函数或类 | 作用 |
|---|---|
| `SensorFrame` | 双板共享的数据结构，包含温湿度、MQ、雨量、热敏、火焰、序号和状态位。 |
| `FrameCodec::encode()` | 把 `SensorFrame` 转换成 22 字节 v2 线缆格式。 |
| `FrameCodec::decode()` | 检查 `AA 55`、长度、版本、校验和，再恢复 `SensorFrame`。 |
| `FrameCodec::checksum()` | 字节累加并返回低 8 位。 |
| `FrameStreamDecoder::push()` | 每次接收一个字节，查找帧边界，返回 `FrameReady`、`BadFrame` 或 `NeedMore`，并在噪声后重新同步。 |

协议 v2 布局：

```text
AA 55 LEN VER TEMP HUMI MQ135 MQ2 RAIN THERM_ADC THERM_C10 FLAME RAIN_WET THERM_HOT SEQ STATUS CHECKSUM
```

`STATUS` 位定义为 `bit0=DHT 错误`、`bit1=热敏 DO 高温`、`bit2=雨量湿态`、`bit3=热敏 ADC 异常`。

## 显示报警节点

| 函数或类 | 作用 |
|---|---|
| `MonitorNode::init()` | 复位显示节点状态，初始化蜂鸣器、PB0/PB1 外接阈值键、OLED 总线/控制器、W25Q64，并打印启动状态。 |
| `MonitorNode::run()` | 协作式主循环：处理接收、扫描按键、推进 Flash 任务、评估报警、更新输出、刷新 OLED、调度日志。 |
| `MonitorNode::processRx()` | 从 `hal::readUsartByte()` 取 USART3 字节，送入 `FrameStreamDecoder`，更新 `latest_frame_`，并打印 JSON schema v2。 |
| `MonitorNode::updateButtons()` | K1 切换 OLED 页面；K2 短按静音 60 秒；PB0 选择 MQ135/MQ2/雨量/热敏；PB1 让当前传感器在 5 档阈值中循环。 |
| `MonitorNode::pressedEdge()` | 对低有效外接阈值键做防抖，并只产生一次按下边沿。 |
| `thresholdsFromLevels()` | 根据四个独立 0..4 传感器档位生成当前 `AlarmThresholds`。 |
| `compatibleThresholdProfile()` | 保留 legacy JSON `thresholdProfile`：`0` 全默认，`1` 全旧灵敏，`2` 全旧宽松，`255` 混合/自定义。 |
| `MonitorNode::updateAlarm()` | 驱动蜂鸣器节奏：危险快速鸣叫，节点离线慢速鸣叫，静音关闭，预警/正常关闭。 |
| `MonitorNode::updateDisplay()` | 使用 `formatMonitorDisplay()` 生成文本，并写入四行 OLED。 |
| `MonitorNode::printFrontendJson()` | 输出一行浏览器可解析的 JSON，包含 schema v2 字段、逐传感器档位、实际阈值、报警状态文本和 legacy `externalRgb` placeholder。 |

## 报警、显示与板级 I/O

| 函数或类 | 作用 |
|---|---|
| `evaluateAlarm()` | 根据最新数据帧、超时、静音窗口和当前逐传感器阈值计算 waiting、lost、danger、warn、muted。 |
| `alarmStateString()` | 把 `AlarmState` 转为 JSON 字符串：`normal`、`warn`、`danger`、`waiting` 或 `node_lost`。 |
| `formatMonitorDisplay()` | 把首页实时读数和第二页当前调节传感器、`1/5..5/5` 档位、实际阈值、Flash 状态格式化为四行 OLED 文本。 |
| `Buzzer::init()` / `Buzzer::set()` | 配置并驱动 `PB8` 上的高电平有源蜂鸣器。 |
| `Buttons::init()` | 把外接阈值按键 `PB0/PB1` 配置为内部上拉输入，按下接 GND 为低电平。 |
| `Buttons::key1Pressed()` / `Buttons::key2Pressed()` | 读取高电平按下的 K1/K2。 |
| `Buttons::thresholdSelectPressed()` / `Buttons::thresholdLevelPressed()` | 读取低有效 PB0/PB1 阈值按键。 |
| `OledDisplay::initBus()` / `initController()` / `clear()` / `printLine()` | 小型 SSD1306 软件 I2C 驱动，用于本地显示。 |

## 可选 Flash 记录器

| 函数或类 | 作用 |
|---|---|
| `W25q64FlashLogger::init()` | 调用 `MX_SPI2_Init()`，通过 HAL SPI 读取 JEDEC ID，检测兼容 W25Q64，并恢复游标元数据。 |
| `W25q64FlashLogger::logFrame()` | 构造一条待写入的 32 字节 v2 记录，包含数据帧、报警状态、tick、四个压缩阈值档位、静音位、记录计数和 CRC。 |
| `W25q64FlashLogger::process()` | 非阻塞状态机，处理扇区擦除、页编程、元数据擦除和元数据写入任务。 |
| `W25q64FlashLogger::present()` | 报告日志功能是否可用。 |
| `W25q64FlashLogger::recordCount()` | 返回 OLED 和 JSON 中显示的累计记录数。 |

## 前端模块

| 模块 | 作用 |
|---|---|
| `frontend/src/parser.ts` | 解析 MONITOR JSON Lines，校验 schema v1/v2，并暴露可本地化的 parser 错误码。 |
| `frontend/src/analysis.ts` | 优先使用固件上报的实际阈值字段，旧日志回退到 `thresholdProfile`，并为看板和 AI 兜底生成本地风险摘要。 |
| `frontend/src/aiProvider.ts` | 提供本地规则聊天和 DeepSeek 直连/代理 provider，并支持本地化 prompt 与兜底回复。 |
| `frontend/src/hooks/useDashboard.ts` | 管理 Web Serial/回放源、历史、事件日志、AI 模式和聊天状态。 |
| `frontend/src/components/TrendChart.tsx` | 渲染 ECharts 趋势图，支持图例、缩放、传感器选中和历史值。 |

## 常见修改入口

| 修改 | 主要文件 |
|---|---|
| 添加传感器数值 | `SensorFrame`、`FrameCodec`、`SensorNode::run()`、`MonitorNode::printFrontendJson()`、前端 parser 和文档 |
| 修改报警阈值 | `App/Config.hpp` 中的 5 档数组和 `thresholdsFromLevels()`，以及 `frontend/src/analysis.ts` 中的旧日志回退/默认值 |
| 修改 OLED 文本 | `formatMonitorDisplay()` |
| 修改 W25Q64 记录格式或 SPI2 初始化 | `W25q64FlashLogger::logFrame()`、`Core/Src/spi.c`、SPI MSP 配置、CRC/元数据策略、文档和未来导出工具 |
