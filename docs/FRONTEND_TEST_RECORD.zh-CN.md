# 前端数据看板测试记录

[English](FRONTEND_TEST_RECORD.en.md) | [返回中文 README](../README.zh-CN.md)

## 测试范围

- 分支：`dev/frontend`
- 固件：`MonitorDebug`、`SensorDebug`
- 前端：静态 Web Serial 页面、JSON Lines 解析器、动态模拟串口、AI 本地洞察和本地对话 provider
- 硬件/真实 LLM：未连接实物或未接入 DeepSeek 时只记录为待测，不伪造结果

## 测试结果

| 项目 | 命令或方式 | 结果 | 备注 |
|---|---|---|---|
| 分支确认 | `git status --short --branch` | 通过 | 当前分支为 `dev/frontend`；`project-introduction.html` 仍为未跟踪文件 |
| 前端全量单测 | `npm --prefix frontend test` | 通过 | 14 项通过，0 项失败；覆盖 parser、analysis、replay source |
| MONITOR 固件配置 | `cmake --preset MonitorDebug` | 通过 | 生成目录 `build/MonitorDebug` |
| MONITOR 固件构建 | `cmake --build --preset MonitorDebug` | 通过 | 本次输出 `ninja: no work to do`，说明现有构建产物已是最新 |
| SENSOR 固件配置 | `cmake --preset SensorDebug` | 通过 | 生成目录 `build/SensorDebug` |
| SENSOR 固件构建 | `cmake --build --preset SensorDebug` | 通过 | 本次输出 `ninja: no work to do`，说明现有构建产物已是最新 |
| 前端本地服务 | `python -m http.server 5173 -d frontend` | 通过 | 本地服务进程已启动并用于浏览器验证 |
| 动态模拟串口 | 浏览器打开 `http://localhost:5173` 并点击“开始模拟” | 通过 | 页面显示“模拟串口”，按钮变为“停止模拟”，事件栏出现 seq 和循环记录 |
| AI 洞察 UI | 模拟串口运行时观察 AI 区域 | 通过 | 风险等级、主要证据、趋势判断、建议动作随最新 JSON 帧更新 |
| AI 本地对话 | 输入“现在安全吗？” | 通过 | 回答引用当前预警状态、MQ135 和 DHT11 证据 |
| 断流状态 | 点击“停止模拟”并等待超过 3 秒 | 通过 | Data 显示“数据已超时”，AI 风险变为“节点离线”，事件栏出现“串口数据断流” |
| 响应式 UI | 浏览器视口 1280px 与 390px | 通过 | 无横向溢出；桌面/移动下 AI、聊天、趋势图均可见 |
| 浏览器控制台 | 读取 error/warning 日志 | 通过 | 0 条 error/warning |
| Web Serial 硬件联调 | 选择板 B CH340C 串口 | 待测 | 需要实物连接 |
| DeepSeek V4-flash 接入 | 后端代理 `/api/ai/chat` | 待接入 | 当前仅预留 provider，不在浏览器保存 API key |

## 模拟数据说明

`frontend/fixtures/sample-serial.log` 同时包含 `[MONITOR]` 普通日志和 JSON Lines。`ReplaySerialSource` 会逐行回放该文件，解析器测试会验证普通日志被忽略、有效 JSON 被接收、非法 JSON 和缺字段会被报告为错误。

## 硬件记录

当前未写入任何硬件或真实 DeepSeek 通过结论。只有实际连接板 B、选择 CH340C 串口并观察到页面实时刷新后，才能把硬件联调结果改为通过；只有接入后端代理并完成真实请求后，才能把 DeepSeek 接入结果改为通过。

## 执行时间

- 记录日期：2026-06-05
- 环境：Windows PowerShell，Node `v24.12.0`，Python `3.13.12`
