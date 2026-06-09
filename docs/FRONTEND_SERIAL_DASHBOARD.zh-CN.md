# Web Serial 前端数据看板

[English](FRONTEND_SERIAL_DASHBOARD.en.md) | [返回中文 README](../README.zh-CN.md)

## 目标

`frontend/` 是板 B MONITOR 节点的浏览器实时看板。板 B 继续通过 USART3 接收板 A 的二进制传感器帧，同时在 USART1 调试口额外输出 JSON Lines。电脑通过目标板的 USB 转串口识别出串口后，Chrome 或 Edge 可以用 Web Serial API 读取这些 JSON Lines 并刷新页面。

数据链路：

```text
Board A SENSOR --USART3 PB10/PB11--> Board B MONITOR --USART1 PA9/PA10 + USB-UART--> Browser Web Serial
```

## 固件输出

板 B 收到有效采集帧后，会保留原有人工可读日志：

```text
[MONITOR] rx v2 seq=31 t=26 h=54 mq135=1020 mq2=860 rain=980 therm=32.5C flame=0 status=0x00
```

同一位置会紧跟一行 JSON：

```json
{"type":"sensor","schemaVersion":2,"seq":31,"tickMs":123456,"tempC":26,"humidityPct":54,"mq135Raw":1020,"mq2Raw":860,"rainRaw":980,"thermRaw":1660,"thermC10":325,"rainWet":0,"thermHot":0,"flame":0,"status":0,"alarm":"normal","thresholdProfile":0,"selectedThresholdSensor":0,"thresholdAirLevel":2,"thresholdSmokeLevel":2,"thresholdRainLevel":2,"thresholdThermLevel":2,"thresholdAirWarn":2200,"thresholdSmokeWarn":1800,"thresholdSmokeDanger":2800,"thresholdRainWet":1400,"thresholdThermWarnC10":450,"thresholdThermDangerC10":700,"mute":0,"flashReady":1,"flashRecords":42,"externalRgb":0}
```

字段说明：

| 字段 | 含义 |
|---|---|
| `type` | 固定为 `sensor` |
| `schemaVersion` | JSON schema 版本；旧固件缺省视为 v1，新固件为 v2 |
| `seq` | 板 A 上报的循环帧序号 |
| `tickMs` | 板 B 输出 JSON 时的 `HAL_GetTick()` |
| `tempC` / `humidityPct` | DHT11 温湿度 |
| `mq135Raw` / `mq2Raw` | MQ135、MQ2 的 12 位 ADC 原始值 |
| `rainRaw` | 雨量模块 12 位 ADC 原始值，v2 字段 |
| `thermRaw` | 热敏 AO 12 位 ADC 原始值，v2 字段 |
| `thermC10` | 热敏查表换算温度，单位 0.1°C，v2 字段 |
| `rainWet` | `1` 表示雨量湿态触发，v2 字段 |
| `thermHot` | `1` 表示热敏 DO 高温触发，v2 字段 |
| `flame` | `1` 表示检测到火焰 |
| `status` | `bit0=DHT11异常`，`bit1=热敏DO高温`，`bit2=雨量触发`，`bit3=热敏ADC异常` |
| `alarm` | `normal`、`warn`、`danger` 或 `node_lost` |
| `thresholdProfile` | legacy 兼容档位：`0` 全默认，`1` 全旧灵敏，`2` 全旧宽松，`255` 混合/自定义 |
| `selectedThresholdSensor` | 当前调节项：`0=MQ135`、`1=MQ2`、`2=RAIN`、`3=THERM` |
| `thresholdAirLevel`、`thresholdSmokeLevel`、`thresholdRainLevel`、`thresholdThermLevel` | 逐传感器档位值 `0..4`；OLED 和文档显示为 `1/5..5/5` |
| `thresholdAirWarn`、`thresholdSmokeWarn`、`thresholdSmokeDanger`、`thresholdRainWet` | 当前实际 ADC 阈值 |
| `thresholdThermWarnC10`、`thresholdThermDangerC10` | 当前热敏阈值，单位 0.1°C |
| `mute` | 蜂鸣器是否处于 K2 短按静音窗口 |
| `flashReady` | 板 B 是否检测到可用 W25Q64 |
| `flashRecords` | W25Q64 环形日志累计记录数，v2 字段 |
| `externalRgb` | 当前固件仍会输出的 legacy placeholder；看板不要求该字段，也不代表 WS2813/RGB 是现役输出 |

前端 parser 同时兼容 v1/v2：没有 `schemaVersion` 的旧 JSON Lines 会按 v1 解析，新字段显示为 `--`；v2 数据则要求雨量、热敏和 Flash 记录数字段齐全。新阈值字段为兼容旧日志保持可选；存在时看板分析优先使用实际阈值，不存在时回退到 `thresholdProfile`。`externalRgb` 之类 placeholder 可被前端忽略。

## 前端运行

在仓库根目录启动 Vite 开发服务器：

```powershell
npm --prefix frontend install
npm --prefix frontend run dev
```

然后用 Chrome 或 Edge 打开：

```text
http://localhost:5173
```

点击“连接串口”，选择板 B 对应的 USB 转串口。串口参数固定为 `115200 8N1`。

## 页面行为

- 页面只解析以 `{` 开头的 JSON Lines，自动忽略 `[MONITOR]`、`[SENSOR]` 等普通日志。
- 首页显示温度、湿度、MQ135、MQ2、雨量、热敏温度、火焰状态和综合告警；运行细节面板显示当前调节传感器、四个阈值档位、实际阈值、静音、Flash 状态和 Flash 记录数。
- 趋势图保留最近 80 条有效传感器记录，包含 ECharts 图例、tooltip、缩放、传感器选中高亮和最近历史值；旧 v1 数据缺少的新曲线会自动跳过。
- 若超过 3 秒没有收到有效 JSON，页面会显示“数据已超时”，但保留最后一次有效值。
- “开始模拟”会按串口节奏逐行回放 `frontend/fixtures/sample-serial.log`，和真实 Web Serial 使用同一条解析链路。
- “AI 洞察”区域基于最近数据给出风险等级、主要证据、趋势判断和建议动作；默认显示 DeepSeek 直连/本地规则混合 provider，本地规则会持续纳入雨量湿态、热敏高温和热敏 ADC 异常。
- “用户对话”区域默认使用页面里填写的 DeepSeek API Key 直连官方 Chat Completions 接口；若直连网络失败，会自动回落到本地规则回答安全、报警原因、MQ2、雨量、热敏和排查类问题。
- 当前 Web Serial 主要支持 Chrome/Edge；其他浏览器会显示不支持提示。

## AI 接入

默认模式是前端直连 DeepSeek 官方 Chat Completions 接口。用户在页面里填写 API Key 后，key 只保存在当前浏览器标签页的 `sessionStorage`，不会写入仓库、URL、本地文件或 `localStorage`；关闭标签页后需要重新填写。

```text
frontend DirectDeepSeekProvider
        -> Authorization: Bearer <user-entered key>
        -> POST https://api.deepseek.com/chat/completions
```

直连请求体只包含官方接口需要的 `model`、带系统提示和传感器快照的 `messages`、`stream`、`temperature`，不会把 API Key 放进 JSON body。默认模型名是 `deepseek-v4-flash`，默认直连接口是 `https://api.deepseek.com/chat/completions`。

如果后续要部署到公网，仍建议改回后端代理模式，因为任何浏览器直连方案都会让当前使用者的 key 出现在浏览器运行时里。可选代理链路如下：

```text
frontend DeepSeekProvider
        -> POST /api/ai/chat
        -> backend proxy with DEEPSEEK_API_KEY
        -> DeepSeek-compatible API
```

代理模式请求体包含 `model`、带系统提示的 `messages`、原始 `conversation`、压缩后的 `snapshot`、`locale` 和 `requestType: "chat"`。

可选配置方式：

```html
<script>
  window.SAFETY_MONITOR_CONFIG = {
    ai: {
      mode: "direct",
      directEndpoint: "https://api.deepseek.com/chat/completions",
      proxyEndpoint: "/api/ai/chat",
      model: "deepseek-v4-flash"
    }
  };
</script>
```

也可以用 URL 参数临时覆盖：`?ai=direct`、`?ai=local`、`?ai=proxy`、`?aiDirectEndpoint=https://api.deepseek.com/chat/completions`、`?aiProxyEndpoint=/api/ai/chat`、`?aiModel=deepseek-v4-flash`。页面右上角的 AI 模式按钮会在直连和本地规则之间切换，并把模式选择保存在浏览器本地存储中。

## CLion/CMake 关系

前端不改变 CLion + CMake 主流程。固件仍按原有预设构建：

```powershell
cmake --preset MonitorDebug
cmake --build --preset MonitorDebug

cmake --preset SensorDebug
cmake --build --preset SensorDebug
```

烧录关系不变：`Env-Monitor_sensor.hex` 烧到板 A，`Env-Monitor_monitor.hex` 烧到板 B；这些名称是当前固件产物名。前端只读取板 B 的 USART1 调试口，不占用 USART3 板间通信。
