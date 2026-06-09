# 项目讲解口径与问答索引

[后端深度讲解](BACKEND_DEEP_DIVE.zh-CN.md) | [前端深度讲解](FRONTEND_DEEP_DIVE.zh-CN.md) | [硬件资料](../hardware/index.zh-CN.md)

本文用于统一项目讲解口径，帮助快速从“功能”追到“模块、引脚、代码和风险边界”。正文刻意使用项目展示、讲解、问答等表述。

## 1. 30 秒总述

这是一个双 STM32F103C8T6 环境安全监测系统。板 A 是采集节点，负责 DHT11、MQ135、MQ2、雨量、热敏和火焰输入；板 B 是监测节点，负责 USART3 流式解码、告警判断、OLED 显示、蜂鸣器提示、阈值按键、W25Q64 历史记录，并通过 USART1 输出 JSON Lines 给浏览器看板。前端看板复用真实串口和模拟回放链路，把实时数据变成指标、趋势、事件和 AI 风险解释。

一句话亮点：

```text
不是简单堆传感器，而是把采集、通信、监测、显示、记录和网页解释连成闭环。
```

## 2. 推荐讲解顺序

1. 先讲系统闭环：传感器 -> 板 A -> USART3 -> 板 B -> OLED/蜂鸣器/Flash/Web。
2. 再讲当前功能：采集、协议、告警、交互、记录、看板、AI 解释。
3. 讲工程特色：双镜像构建、`22 字节 v2` 协议、流式重同步、阈值可调、可选 Flash、前端回放。
4. 用依赖矩阵说明每个功能依赖什么模块、引脚和代码。
5. 展示硬件模块与原理图来源：项目文档负责当前映射，Wildfire 资料负责原图证据。
6. 展示关键代码：`SensorNode::run()`、`FrameCodec`、`FrameStreamDecoder::push()`、`MonitorNode::run()`、`useDashboard()`。
7. 展示前端页面：指标卡、趋势图、详情、事件、AI 助手。
8. 最后讲验证和边界：测试、构建、回放、标定、电气限制。

## 3. 功能到模块/代码矩阵

| 用户可见功能 | 硬件/模块 | 引脚 | 后端入口 | 前端入口 |
|---|---|---|---|---|
| 温湿度采集 | DHT11 | SENSOR `PB12` | `Dht11::read()`、`SensorNode::run()` | `SensorRecord.tempC/humidityPct`、`MetricGrid` |
| 空气质量趋势 | MQ135 | SENSOR `PA4/ADC1_CH4` | `hal::readAdc1Channel(4)`、`filter()` | `mq135Raw`、`TrendChart`、`analysis.ts` |
| 烟雾预警/危险 | MQ2 | SENSOR `PA5/ADC1_CH5` | `hal::readAdc1Channel(5)`、`AlarmEvaluator` | `mq2Raw`、AI 风险原因 |
| 雨量湿态 | 雨量模块 | SENSOR `PA6/ADC1_CH6` | `rain_adc`、`rain_wet` 状态位 | `rainRaw/rainWet`、详情面板 |
| 热敏温度 | NTC/LM393 | SENSOR `PA7/ADC1_CH7`、`PB9` | `thermistorAdcToC10()`、`therm_hot` | `thermC10/thermHot` |
| 火焰危险 | 火焰模块 | SENSOR `PB13` | `frame.flame`、`hasDangerCondition()` | `flame`、风险等级 |
| 双板通信 | USART3 | `PB10/PB11` 交叉 | `FrameCodec`、`FrameStreamDecoder` | 不直接接触，由板 B 转 JSON |
| 本地显示 | SSD1306 OLED | MONITOR `PB6/PB7` | `OledDisplay`、`DisplayFormatter` | 无 |
| 声音提示 | 有源蜂鸣器 | MONITOR `PB8` | `Buzzer::set()`、`updateAlarm()` | 显示 `mute/alarm` |
| 阈值调节 | K1/K2/PB0/PB1 | `PA0/PC13/PB0/PB1` | `updateButtons()`、`thresholdsFromLevels()` | `selectedThresholdSensor` 和阈值字段 |
| 历史记录 | W25Q64 | MONITOR `PB12..PB15` | `W25q64FlashLogger` | `flashReady/flashRecords` |
| 浏览器看板 | CH340C/Web Serial | USART1 `PA9/PA10` | `printFrontendJson()` | `WebSerialSource`、`parseSerialLine()` |
| 模拟演示 | 样例日志 | 无硬件 | 固件输出格式样例 | `ReplaySerialSource` |
| AI 解释 | 本地规则/DeepSeek | 浏览器网络 | JSON 字段提供事实 | `analysis.ts`、`aiProvider.ts` |

## 4. 关键代码怎么讲

### `SensorNode::run()`

讲法：

- 每秒进一次采样分支。
- 四路 ADC 读取 MQ135、MQ2、雨量、热敏 AO。
- DHT11 用独立周期，避免读得太频繁。
- 第一次样本直接作为平均值，后续用 3:1 平滑。
- 热敏 ADC 查表换算成 0.1 摄氏度，并检测近 0/满量程异常。
- 最终填入 `SensorFrame`，由 `FrameCodec` 打包发到 USART3。

一句话结论：

```text
板 A 的价值是把多种不规则输入统一成稳定、固定格式的一帧。
```

### `FrameCodec` + `FrameStreamDecoder`

讲法：

- `FrameCodec` 规定 `AA 55` 帧头、长度、版本、字段偏移和 checksum。
- 固定 `22 字节 v2` 能让接收端知道何时凑够一帧。
- `FrameStreamDecoder::push()` 每来一个字节推进一次，不需要一次性读完整帧。
- 遇到噪声、坏校验、重叠帧头能重新同步。

一句话结论：

```text
协议层让普通串口字节流变成可恢复、可测试的数据帧。
```

### `MonitorNode::run()`

讲法：

- 主循环没有 RTOS，而是协作式调度。
- 每轮先处理接收和按键，再推进 Flash 状态机。
- 告警、OLED、Flash 写入按各自周期运行。
- Flash 可用才写，不可用不影响主监测。

一句话结论：

```text
板 B 把同一份数据分发到声音、屏幕、存储和网页四个出口。
```

### `useDashboard()`

讲法：

- 是前端状态中枢。
- 真实串口和回放最终都调用 `ingestLine()`。
- `parseSerialLine()` 成功后更新 `latest/history/events`。
- `buildAnalysisSnapshot()` 把当前状态变成风险原因、趋势和建议。
- 所有组件都从同一个 `dashboard` 对象读数据。

一句话结论：

```text
前端不是另写一套假数据，而是复用板 B 的真实 JSON 数据链路。
```

## 5. 工程特色口径

| 特色 | 怎么讲 | 证据 |
|---|---|---|
| 双镜像构建 | 同一仓库、同一主入口，通过 `APP_NODE_ROLE` 变成两块板的固件。 | `CMakeLists.txt`、`main.cpp` |
| 固定二进制协议 | SENSOR 到 MONITOR 不发散乱文本，而发 `22 字节 v2` 帧。 | `FrameCodec.cpp`、协议测试 |
| 流式重同步 | MONITOR 不怕前面有噪声或坏帧，可重新找到下一帧。 | `FrameStreamDecoder.cpp`、测试 |
| 告警优先级 | Danger、Waiting、Lost、Warn、Normal 顺序明确。 | `AlarmEvaluator.cpp` |
| 阈值可调 | PB0 选择传感器，PB1 调整 5 档阈值。 | `Config.hpp`、`updateButtons()` |
| 退化运行 | W25Q64 不在时仍可监测、显示、报警和输出 JSON。 | `W25q64FlashLogger::present()` |
| 前端回放 | 无硬件也能验证 parser、状态、图表和 AI 面板。 | `ReplaySerialSource`、fixture |
| AI 解释 | DeepSeek/本地规则把风险原因和建议动作讲清楚。 | `analysis.ts`、`aiProvider.ts` |

## 6. 硬件资料引用口径

项目内文档负责“当前项目怎么用”：

- `WIRING.md`
- `docs/hardware/index.zh-CN.md`
- `docs/hardware/modules/*.zh-CN.md`
- `App/BoardPins.hpp`

Wildfire 本地资料库负责“原理图和模块证据”：

```text
C:\baidunetdiskdownload\野火小智STM32F103C8_HAL库实战资料与课件
```

讲解时不要把课程例程的接线图直接当成当前项目接线。当前项目引脚以 `WIRING.md` 和 `BoardPins.hpp` 为准，原理图截图只作为模块电路证据。

## 7. 容易混淆的点

| 混淆点 | 正确口径 |
|---|---|
| `.ioc` 等于最终代码 | 不完全等于。当前 ADC、USART 等有手写配置，应以源码为准。 |
| `PB12/PB13` 冲突 | 不冲突。SENSOR 和 MONITOR 是两块板、两个镜像。 |
| `externalRgb` 是现役输出 | 不是。它是 legacy placeholder，不作为现役功能展示。 |
| W25Q64 必须接 | 不是。它是已实现的可选记录能力。 |
| MQ 原始值就是 ppm | 不是。ppm 需要标定，当前展示 ADC 趋势和阈值。 |
| 前端会控制硬件 | 不会。前端读取板 B USART1 JSON Lines，硬件控制在固件中。 |
| AI 直接判断硬件安全 | AI 是解释层，依据固件和前端快照给原因与建议。 |

## 8. 常见问答

| 问题 | 回答 |
|---|---|
| 为什么不用一块板完成所有事？ | 两块板能把采集和监测职责拆开，板 A 专注现场输入，板 B 专注显示、告警、记录和上位机输出，链路更清晰。 |
| 为什么板间不用 JSON？ | 板间链路更适合固定二进制帧，长度短、解析简单、恢复边界明确；JSON 放在板 B 到浏览器这条人机可读链路。 |
| 为什么要 `seq`？ | 用来观察帧是否连续，也方便日志和前端定位丢帧。 |
| 为什么 DHT11 不是每秒读？ | DHT11 本身不适合高频读取，项目用 `dht11_period_ms` 控制周期。 |
| 为什么 Warn 不响蜂鸣器？ | 当前策略是危险和节点离线才用声音提示，预警交给 OLED 和前端显示，避免提示过于频繁。 |
| 为什么 Lost 慢速响？ | 它不是传感器危险，而是采集链路异常，需要提示但不和危险态混淆。 |
| 为什么前端有 stale？ | 超过 3 秒没有新 JSON 时，不能把最后一帧当实时状态，所以 UI 单独提示数据超时。 |
| 为什么有本地 AI 规则？ | 网络或 API 不可用时，仍能基于阈值和状态位给出风险解释。 |

## 9. 结束总结

可以用三句话收束：

1. 后端完成了从多传感器输入到双板可靠通信、告警、显示和记录的闭环。
2. 前端复用板 B 的真实 JSON Lines，把数据变成可视化趋势、事件和 AI 风险解释。
3. 项目的关键工程价值在于职责分离、固定协议、状态优先级、可选记录和可验证的数据链路。
