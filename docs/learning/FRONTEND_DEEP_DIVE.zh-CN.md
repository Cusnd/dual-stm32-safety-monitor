# 前端深度讲解

[返回 README](../../README.zh-CN.md) | [前端说明](../FRONTEND_SERIAL_DASHBOARD.zh-CN.md)

本文面向没有 JavaScript、TypeScript、React 基础的读者，目标是讲清楚浏览器看板怎样把板 B 的 USART1 JSON Lines 变成指标卡、趋势图、事件日志和 AI 风险解释。前端不控制硬件，只读取板 B 已输出的数据。

## 1. 前端一句话

`frontend/` 是一个 Vite + React + TypeScript 浏览器看板。它有两种输入源：

```text
真实串口：Board B USART1 JSON Lines -> WebSerialSource
模拟回放：fixtures/sample-serial.log -> ReplaySerialSource
```

两条链路最终都进入：

```text
useDashboard.ingestLine()
  -> parseSerialLine()
  -> SensorRecord
  -> latest/history/events
  -> buildAnalysisSnapshot()
  -> MetricGrid / TrendChart / DetailsPanel / EventsPanel / AiPanel / ChatPanel
```

## 2. 工程入口与运行

| 文件 | 分类 | 作用 |
|---|---|---|
| `frontend/package.json` | 配置 | npm 脚本和依赖。`dev` 启动 Vite，`build` 先跑 TypeScript 再打包，`test` 跑 Vitest。 |
| `frontend/index.html` | HTML 入口 | 提供根节点，Vite 把 React 应用挂载进去。 |
| `frontend/src/main.tsx` | React 入口 | 创建 Mantine 主题，把 `<App />` 渲染到 DOM。 |
| `frontend/src/App.tsx` | 页面骨架 | 组织 Header、状态条、指标卡、趋势图、事件、详情和浮动 AI 助手。 |
| `frontend/vite.config.ts` | Vite 配置 | React 插件、开发/测试环境配置。 |
| `frontend/tsconfig.json` / `tsconfig.node.json` | TypeScript 配置 | 控制类型检查范围和 Node 侧配置文件编译。 |
| `frontend/package-lock.json` | 锁定依赖版本 | 只需说明用途，不需要逐包讲解。 |
| `frontend/fixtures/sample-serial.log` | 样例数据 | 模拟板 B 串口输出，回放和测试共用。 |

关键依赖：

| 依赖 | 用途 |
|---|---|
| `react` / `react-dom` | UI 组件和渲染。 |
| `@mantine/core` / `@mantine/hooks` / `@mantine/notifications` | 页面布局、控件、交互工具。 |
| `echarts` / `echarts-for-react` | 趋势图。 |
| `lucide-react` | 图标。 |
| `react-markdown` / `remark-gfm` | AI 聊天 Markdown 渲染。 |
| `vitest` / Testing Library / jsdom | 单元测试和组件测试。 |

## 3. TypeScript 数据模型：`frontend/src/types.ts`

TypeScript 的重点是“先把数据长什么样说清楚”。本项目所有模块都围绕这些类型工作。

| 类型 | 含义 | 学习重点 |
|---|---|---|
| `Locale` | `"zh-CN"` 或 `"en"`。 | 语言切换只有两种合法值。 |
| `AlarmState` | `"normal" / "warn" / "danger" / "node_lost"`。 | 前端没有 `waiting` 类型，等待数据通过 `RiskLevel` 表达。 |
| `RiskLevel` | `"unknown" / "normal" / "warning" / "danger" / "offline"`。 | AI/本地分析层的风险等级。 |
| `AiMode` | `"direct" / "proxy" / "local"`。 | DeepSeek 直连、后端代理、本地规则。 |
| `SourceMode` | `"idle" / "serial" / "replay"`。 | 当前输入源。 |
| `SourceStatus` | `"connected" / "disconnected" / "loop"`。 | 输入源回调给 dashboard 的状态。 |
| `EventType` | 事件类型。 | 事件面板颜色和图标依赖它。 |
| `ThresholdProfile` | 一组实际阈值。 | 与固件 `AlarmThresholds` 对应。 |
| `SensorRecord` | 一条解析成功的传感器记录。 | 字段来自板 B JSON Lines，是前端最核心数据。 |
| `ParseResult` | 解析结果联合类型。 | `ignored/error/sensor` 三类，调用方必须分支处理。 |
| `AnalysisSnapshot` | 本地分析快照。 | 包含 latest、history、风险、原因、趋势、建议。 |
| `Insight` | AI 面板展示内容。 | provider 可以是本地或 DeepSeek。 |
| `ChatMessage` | 聊天消息。 | `pending` 表示等待回复。 |
| `BackendMessage` | 发给 AI 接口的精简消息。 | 只保留 role/content。 |
| `AssistantReply` | AI 回复。 | 带 provider 和可选 fallbackReason。 |
| `AiProvider` | AI provider 接口。 | 任何 provider 必须实现 `analyze()` 和 `chat()`。 |
| `AiConfig` | AI 配置。 | 包含 mode、endpoint、model、timeout、fallback。 |
| `EventRow` | 事件列表一行。 | 包含 id、time、message、type。 |
| `SerialSourceCallbacks` | 输入源回调。 | `onLine/onStatus/onError`。 |
| `MetricDefinition` | 指标卡定义。 | key、文案 key、单位和色调。 |
| `ChartSeriesDefinition` | 趋势图序列定义。 | key、单位、颜色、y 轴和值提取函数。 |
| `RuntimeSafetyMonitorConfig` | `window.SAFETY_MONITOR_CONFIG` 形状。 | 允许 HTML 或部署环境覆盖 AI 配置。 |

`SensorRecord` 中的可选字段来自新固件的逐传感器阈值扩展；`rainRaw/thermRaw/thermC10/flashRecords` 允许 `null`，是为了兼容旧 schema。

## 4. 常量与格式化

### `frontend/src/constants.ts`

| 导出/函数 | 作用 |
|---|---|
| `MAX_HISTORY` | 趋势图保留最近 80 条记录。 |
| `MAX_EVENTS` | 事件列表最多 12 条。 |
| `MAX_CHAT_MESSAGES` | 聊天上下文最多 12 条。 |
| `STALE_AFTER_MS` | 超过 3 秒没有有效数据，页面进入 stale。 |
| `metricDefinitions` | 指标卡配置，驱动 `MetricGrid` 渲染。 |
| `numericValue()` | 从 `SensorRecord` 中安全取数值，非数值返回 `null`。 |
| `chartSeriesDefinitions` | 趋势图序列配置，驱动 `TrendChart`。 |

### `frontend/src/format.ts`

| 函数 | 作用 |
|---|---|
| `eventTone()` | 把事件或告警类型转成视觉 tone。 |
| `eventLabel()` | 把事件类型本地化成中文/英文。 |
| `seriesTitle()` | 根据语言生成图表序列名称。 |
| `seriesByKey()` | 根据 key 查找序列定义。 |
| `formatSeriesValue()` | 格式化趋势图 tooltip/历史值。 |
| `formatMetricValue()` | 格式化指标卡显示值，支持 stale 和缺失值。 |

### `frontend/src/i18n.ts`

| 导出/函数 | 作用 |
|---|---|
| `strings` | 中英双语文案表。 |
| `TranslationKey` | 文案 key 的类型。 |
| `translate(locale, key)` | 安全取本地化文案。 |
| `useLocaleText(locale)` | 返回绑定语言的翻译函数。 |
| `formatClock(locale, timestamp)` | 按语言格式化时间。 |

## 5. 配置读取：`frontend/src/config.ts`

| 函数/常量 | 作用 | 易错点 |
|---|---|---|
| `DEFAULT_AI_CONFIG` | 默认 AI 配置。 | 默认可使用直连或本地规则，具体由配置解析决定。 |
| `runtimeConfig()` | 读取 `window.SAFETY_MONITOR_CONFIG`。 | 部署时可在 HTML 中注入。 |
| `searchParams()` | 读取 URL 查询参数。 | 便于临时切换 `?ai=local` 等。 |
| `storedMode()` | 从 `localStorage` 读取 AI 模式。 | 只保存模式，不保存 API key。 |
| `normalizeAiMode()` | 把未知输入归一化为合法模式。 | 非法值回默认安全模式。 |
| `numberValue()` | 数字配置解析。 | 非法值回 fallback。 |
| `resolveAiConfig()` | 合并默认、运行时配置、URL、localStorage。 | 这是 AI 配置入口。 |
| `persistAiMode()` | 保存 AI 模式到 `localStorage`。 | 影响下次打开页面。 |
| `getDirectApiKey()` | 从 `sessionStorage` 读取直连 key。 | 关闭标签页后消失。 |
| `hasDirectApiKey()` | 判断是否存在直连 key。 | UI 用于提示。 |
| `persistDirectApiKey()` | 保存 key 到 `sessionStorage`。 | 不写入仓库、URL 或本地文件。 |
| `clearDirectApiKey()` | 清除 key。 | 切换/清理时使用。 |

## 6. JSON Lines 解析器：`frontend/src/parser.ts`

解析器只关心以 `{` 开头的行，普通 `[MONITOR]` 日志会被忽略。

| 常量/函数 | 作用 |
|---|---|
| `REQUIRED_NUMBERS` | v1/v2 都必须有的数字字段。 |
| `V2_REQUIRED_NUMBERS` | schema v2 必须有的雨量、热敏、Flash 字段。 |
| `OPTIONAL_THRESHOLD_NUMBERS` | 新阈值字段，可选但出现时必须是整数。 |
| `ALARM_STATES` | 前端允许的 alarm 字符串集合。 |
| `toInteger(value, field)` | 把未知值转整数，不合法就抛错。 |
| `normalizeSensorRecord(record)` | 校验 JSON 对象并生成 `SensorRecord`。 |
| `parseSerialLine(line)` | 解析一行文本，返回 `ParseResult`。 |
| `parseSerialText(text)` | 批量解析多行文本，过滤 ignored。 |
| `alarmLabel(alarm, locale)` | 告警状态本地化。 |
| `parserErrorLabel(error, locale)` | 把内部错误转成用户可读提示。 |

关键兼容规则：

- 没有 `schemaVersion` 的旧日志按 v1。
- v2 必须包含 `rainRaw`、`thermRaw`、`thermC10`、`rainWet`、`thermHot`、`flashRecords`。
- 新阈值字段是可选的；存在时优先用于分析。
- `externalRgb` 可被忽略，不代表现役输出。

## 7. 输入源

### `frontend/src/serial.ts`

| 类型/函数 | 作用 |
|---|---|
| `SerialPortLike` | 抽象浏览器串口对象，便于测试和类型检查。 |
| `SerialNavigator` | 给 `navigator` 加上可选 `serial` 字段。 |
| `WebSerialSource` | 真实 Web Serial 输入源。 |
| `constructor()` | 保存 `onLine/onStatus/onError` 回调。 |
| `isSupported()` | 检查浏览器是否支持 Web Serial。 |
| `connect()` | 请求串口、以 `115200 8N1` 打开、启动读循环。 |
| `disconnect()` | 取消 reader、释放锁、关闭 port、发出 disconnected。 |
| `readLoop()` | 从 `ReadableStream` 连续读 `Uint8Array`。 |
| `pushText(chunk)` | 文本缓冲拆行，只把完整行交给 `onLine`。 |

### `frontend/src/replaySerial.ts`

| 类型/函数 | 作用 |
|---|---|
| `TimerApi` | 抽象 `setTimeout/clearTimeout`，便于测试。 |
| `ReplaySerialSource` | 模拟串口输入源。 |
| `constructor()` | 配置回调、间隔、是否循环、fixture URL 或内联文本。 |
| `connect()` | 加载文本、拆行、开始定时发送。 |
| `disconnect()` | 清 timer，停止回放。 |
| `loadText()` | fetch 样例日志。 |
| `scheduleNext()` | 安排下一行。 |
| `clearTimer()` | 清理当前 timer。 |
| `emitNext()` | 发出一行，必要时循环或停止。 |

真实串口和回放源都只负责“吐出文本行”，不直接更新 UI。这样测试和页面行为保持一致。

## 8. 本地风险分析：`frontend/src/analysis.ts`

| 常量/函数 | 作用 |
|---|---|
| `THRESHOLD_PROFILES` | legacy 三档阈值。 |
| `messages` | 中英双语分析文案。 |
| `msg(locale)` | 取当前语言文案。 |
| `thresholdForProfile(profile)` | legacy profile 到阈值。 |
| `thresholdsForRecord(record)` | 优先使用记录里的实际阈值，否则回退 profile。 |
| `recentDelta(history, key)` | 最近 5 条的变化量。 |
| `addTrend()` | 超过最小变化才加入趋势描述。 |
| `c10ToC()` | 0.1 摄氏度转显示字符串。 |
| `buildAnalysisSnapshot()` | 生成风险等级、原因、趋势、建议。 |

优先级口径：

1. 没有数据：`unknown`。
2. stale 或 `node_lost`：`offline`。
3. 火焰、MQ2 危险、热敏 DO、热敏危险温度：`danger`。
4. MQ135/MQ2 预警、雨量、热敏预警、DHT/热敏 ADC 状态位：`warning`。
5. 没有触发：`normal`。

前端分析是“解释层”，固件 `alarm` 是“设备上报层”。文档应说明二者会互相参考，但不是同一个函数。

## 9. AI provider：`frontend/src/aiProvider.ts`

| 函数/类 | 作用 |
|---|---|
| `phrases` | 中英双语 AI/本地规则短语。 |
| `p(locale)` | 获取短语。 |
| `lastUserText()` | 找最近一条用户消息。 |
| `safeMessage()` | 把聊天消息压缩为后端需要的 role/content。 |
| `compactFrame()` | 压缩 `SensorRecord`，避免传太多无关字段。 |
| `compactSnapshot()` | 压缩分析快照。 |
| `systemPrompt()` | 构造系统提示，约束回答风格和安全边界。 |
| `buildBackendMessages()` | 构造发给 AI 的消息数组。 |
| `extractAssistantContent()` | 从兼容 Chat Completions 的响应里提取文本。 |
| `normalizeAssistantReply()` | 统一 AI 回复对象。 |
| `withStatusError()` | 给 Error 附带 HTTP status。 |
| `LocalInsightProvider` | 本地规则 provider，离线可用。 |
| `DeepSeekProvider` | 代理模式 provider，请求 `/api/ai/chat`。 |
| `DirectDeepSeekProvider` | 浏览器直连 DeepSeek 兼容接口。 |

安全边界：

- 直连 key 只放 `sessionStorage`，但浏览器直连方案仍会让当前使用者在运行时看到自己的 key。
- 网络失败可回退本地规则。
- 本地规则适合解释当前风险，不负责替代硬件标定或安全处置。

## 10. 状态中枢：`frontend/src/hooks/useDashboard.ts`

这是前端最重要的文件。组件基本不直接管理业务逻辑，而是读取 `useDashboard()` 返回的 `DashboardState`。

### 内部 helper

| 函数 | 作用 |
|---|---|
| `makeId(prefix)` | 生成事件和聊天消息 id。优先用 `crypto.randomUUID()`。 |
| `createAiProvider(config)` | 根据 `AiConfig.mode` 创建本地、直连或代理 provider。 |

### React state

| 状态 | 作用 |
|---|---|
| `locale` | 当前语言。 |
| `sourceMode` | 当前输入源：idle/serial/replay。 |
| `latest` | 最新一条有效 `SensorRecord`。 |
| `stale` | 是否超过 3 秒无新数据。 |
| `history` | 最近 80 条记录。 |
| `events` | 最近 12 条事件。 |
| `chatMessages` | 聊天上下文。 |
| `chatBusy` | 是否等待 AI 回复。 |
| `selectedSeriesKey` | 当前图表选中序列。 |
| `aiConfig` | AI 配置。 |
| `apiKeyVersion` | key 变化触发 provider 重建。 |
| `serialRef` / `replayRef` | 保存当前输入源实例。 |

### 核心函数

| 函数 | 作用 |
|---|---|
| `t(key)` | 绑定当前语言的翻译函数。 |
| `formatSourceError(error)` | 把串口/回放错误变成用户提示。 |
| `addEvent(message, type)` | 插入事件并限制长度。 |
| `ingestLine(line)` | 解析一行文本，成功则更新 latest/history/events，失败则写错误事件。 |
| `stopReplay()` | 停止回放并回到 idle。 |
| `disconnectSerial()` | 断开真实串口。 |
| `connectSerial()` | 停止回放，创建 `WebSerialSource`，请求串口连接。 |
| `startReplay()` | 断开真实串口，创建 `ReplaySerialSource`。 |
| `toggleReplay()` | 开/关回放。 |
| `setAiMode(mode)` | 切换 AI 模式并保存。 |
| `saveApiKey(apiKey)` | 保存直连 key 到 sessionStorage。 |
| `clearApiKey()` | 清除直连 key。 |
| `aiProvider` | `useMemo` 创建 provider。 |
| `snapshot` | `useMemo` 调 `buildAnalysisSnapshot()`。 |
| `insight` | provider 对 snapshot 的本地分析结果。 |
| `sendChat(content)` | 追加用户消息、显示 pending、调用 provider、写回回复。 |

### effect 与返回值

`useDashboard` 还负责：

- stale 定时器：超过 `STALE_AFTER_MS` 标记数据超时。
- 卸载清理：断开串口和回放。
- 返回给 UI 的所有状态、翻译函数和操作函数。

理解这个 hook，就等于理解前端业务流。

## 11. 页面组件

| 文件/组件 | 作用 |
|---|---|
| `App.tsx` | 页面总装配，设置 `<AppShell>`、Header、主内容和浮动助手。 |
| `HeaderControls.tsx` | 顶部按钮：连接串口、模拟回放、语言切换、AI 模式等。 |
| `StatusStrip.tsx` | 连接状态、数据状态、最新时间。 |
| `MetricGrid.tsx` | 根据 `metricDefinitions` 渲染指标卡。 |
| `TrendChart.tsx` | ECharts 趋势图，支持多序列、双 y 轴、tooltip、markArea、选中序列。 |
| `EventsPanel.tsx` | 事件列表，使用 `eventTone()` 决定图标和颜色。 |
| `DetailsPanel.tsx` | 展示阈值、状态位、静音、Flash、当前调节项。 |
| `AiPanel.tsx` | 展示风险等级、摘要、原因、趋势、建议和 AI 设置入口。 |
| `ChatPanel.tsx` | 聊天输入和消息列表，支持 pending 状态和自动滚动。 |
| `DsAssistant.tsx` | 右下角浮动 AI 助手，管理打开/关闭、动画、尺寸和 Escape。 |
| `MarkdownMessage.tsx` | 安全渲染 Markdown，过滤图片和危险链接。 |

组件学习方法：先看 props。几乎所有组件都只接收 `dashboard: DashboardState`，然后从里面读取需要的数据。

## 12. 样式：`frontend/src/styles.css`

样式文件负责：

- 深色工程风主题。
- Header、容器、指标卡、图表、事件、详情面板布局。
- 浮动 AI 助手尺寸、动画和响应式。
- Markdown 消息样式。
- 小屏幕下的布局收缩。

修改样式时要检查移动端，尤其 Header 高度、按钮换行、图表高度和浮窗最小宽度。

## 13. 测试映射

| 测试文件 | 覆盖内容 |
|---|---|
| `frontend/tests/setup.ts` | jsdom 环境，mock `ResizeObserver`、`matchMedia`，清理存储。 |
| `parser.test.ts` | 普通日志忽略、v1/v2、缺字段、非法 JSON、标签、fixture。 |
| `analysis.test.ts` | 无数据、MQ2、实时阈值、DHT、雨量、热敏、断流、本地问答。 |
| `replay-source.test.ts` | 回放逐行、循环、断开清 timer。 |
| `ai-provider.test.ts` | 代理请求体、兜底、纯文本响应、直连 key header、缺 key。 |
| `app.test.tsx` | 语言切换、回放入 UI、图表选择、浮窗、聊天状态。 |
| `markdown.test.tsx` | Markdown/GFM、安全链接、禁用图片。 |

测试证明的是前端逻辑和组件行为。ECharts 在测试中通常被 mock，不代表真实 canvas 渲染质量；真实截图仍要用浏览器检查。

## 14. 修改入口速查

| 想修改 | 主要文件 | 同步点 |
|---|---|---|
| 固件新增 JSON 字段 | `types.ts`、`parser.ts`、`analysis.ts` | 后端 `printFrontendJson()`、测试、文档。 |
| 新增指标卡 | `constants.ts`、`MetricGrid.tsx` | `formatMetricValue()`。 |
| 新增趋势线 | `constants.ts`、`TrendChart.tsx` | 颜色、y 轴、tooltip。 |
| 改风险规则 | `analysis.ts` | `aiProvider.ts` 本地回答、测试。 |
| 改 AI 模式默认值 | `config.ts` | UI 文案、测试。 |
| 改串口参数 | `serial.ts` | 后端 USART1 输出参数必须一致。 |
| 改回放样例 | `fixtures/sample-serial.log` | parser 和 app 测试。 |
| 改中英文文案 | `i18n.ts` | `TranslationKey` 会帮助发现漏项。 |

## 15. 高频追问口径

| 问题 | 建议回答 |
|---|---|
| 前端为什么能读串口？ | Chrome/Edge 提供 Web Serial API，页面请求用户选择串口，以 `115200 8N1` 读取板 B 的 USART1 文本流。 |
| 为什么普通日志不会报错？ | `parseSerialLine()` 只解析以 `{` 开头的 JSON Lines，普通日志返回 ignored。 |
| 为什么要有回放模式？ | 没有硬件时也能验证 parser、状态管理、图表、AI 面板和组件交互。 |
| 前端风险和固件告警是否一样？ | 固件上报 `alarm` 是设备侧结果，前端 `buildAnalysisSnapshot()` 是解释层，会结合 stale、历史趋势和阈值字段生成说明。 |
| API key 放在哪里？ | 直连模式只放在当前标签页的 `sessionStorage`，关闭后消失；公网部署仍建议用代理模式。 |
| 为什么 v2 字段很严格？ | 前端需要雨量、热敏、Flash 等字段支撑页面和分析。缺字段说明固件和前端协议版本不匹配。 |
