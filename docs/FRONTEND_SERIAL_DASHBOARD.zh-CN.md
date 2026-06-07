# Web Serial 前端数据看板

[English](FRONTEND_SERIAL_DASHBOARD.en.md) | [返回中文 README](../README.zh-CN.md)

## 目标

`frontend/` 是板 B MONITOR 节点的浏览器实时看板。板 B 继续通过 USART3 接收板 A 的二进制传感器帧，同时在 USART1 调试口额外输出 JSON Lines。电脑通过板载 CH340C 识别出串口后，Chrome 或 Edge 可以用 Web Serial API 读取这些 JSON Lines 并刷新页面。

数据链路：

```text
Board A SENSOR --USART3 PB10/PB11--> Board B MONITOR --USART1 PA9/PA10 + CH340C--> Browser Web Serial
```

## 固件输出

板 B 收到有效采集帧后，会保留原有人工可读日志：

```text
[MONITOR] rx seq=31 t=26 h=54 mq135=1020 mq2=860 flame=0 status=0x00
```

同一位置会紧跟一行 JSON：

```json
{"type":"sensor","seq":31,"tickMs":123456,"tempC":26,"humidityPct":54,"mq135Raw":1020,"mq2Raw":860,"flame":0,"status":0,"alarm":"normal","thresholdProfile":0,"mute":0,"flashReady":1}
```

字段说明：

| 字段 | 含义 |
|---|---|
| `type` | 固定为 `sensor` |
| `seq` | 板 A 上报的循环帧序号 |
| `tickMs` | 板 B 输出 JSON 时的 `HAL_GetTick()` |
| `tempC` / `humidityPct` | DHT11 温湿度 |
| `mq135Raw` / `mq2Raw` | MQ135、MQ2 的 12 位 ADC 原始值 |
| `flame` | `1` 表示检测到火焰 |
| `status` | 板间帧状态位，`bit0 = DHT11 读取异常` |
| `alarm` | `normal`、`warn`、`danger` 或 `node_lost` |
| `thresholdProfile` | K2 长按切换的阈值档位 |
| `mute` | 蜂鸣器是否处于 K2 短按静音窗口 |
| `flashReady` | 板 B 是否检测到可用 W25Q64 |

## 前端运行

在仓库根目录启动静态服务器：

```powershell
python -m http.server 5173 -d frontend
```

然后用 Chrome 或 Edge 打开：

```text
http://localhost:5173
```

点击“连接串口”，选择板 B 对应的 CH340C 串口。串口参数固定为 `115200 8N1`。

## 页面行为

- 页面只解析以 `{` 开头的 JSON Lines，自动忽略 `[MONITOR]`、`[SENSOR]` 等普通日志。
- 首页显示温度、湿度、MQ135、MQ2、火焰状态、综合告警、阈值档位、静音和 Flash 状态。
- 趋势图保留最近 80 条有效传感器记录。
- 若超过 3 秒没有收到有效 JSON，页面会显示“数据已超时”，但保留最后一次有效值。
- “开始模拟”会按串口节奏逐行回放 `frontend/fixtures/sample-serial.log`，和真实 Web Serial 使用同一条解析链路。
- “AI 洞察”区域基于最近数据给出风险等级、主要证据、趋势判断和建议动作；当前版本使用本地规则 provider。
- “用户对话”区域会用当前传感器快照回答安全、报警原因、MQ2 和排查类问题。
- 当前 Web Serial 主要支持 Chrome/Edge；其他浏览器会显示不支持提示。

## AI 接入预留

当前前端不会直接调用 DeepSeek，也不会保存 API key。后续接入 DeepSeek V4-flash 时，建议新增本地或云端后端代理，例如：

```text
frontend LocalInsightProvider / DeepSeekProvider
        -> POST /api/ai/chat
        -> DeepSeek-compatible API
```

前端已经预留 `AiProvider` 接口：`analyze(snapshot)` 和 `chat({ messages, snapshot, locale })`。正式接入时替换 provider 即可，传感器 JSON Lines 和页面布局不需要改。

## CLion/CMake 关系

前端不改变 CLion + CMake 主流程。固件仍按原有预设构建：

```powershell
cmake --preset MonitorDebug
cmake --build --preset MonitorDebug

cmake --preset SensorDebug
cmake --build --preset SensorDebug
```

烧录关系不变：`Fire_F103_sensor.hex` 烧到板 A，`Fire_F103_monitor.hex` 烧到板 B。前端只读取板 B 的 USART1 调试口，不占用 USART3 板间通信。
