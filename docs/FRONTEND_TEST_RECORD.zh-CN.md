# 前端数据看板测试记录

[English](FRONTEND_TEST_RECORD.en.md) | [返回中文 README](../README.zh-CN.md)

## 测试范围

- 分支：`main`
- 固件：`MonitorDebug`、`SensorDebug`
- 前端：Vite + React + TypeScript Web Serial 看板、Mantine 布局、ECharts 趋势图、JSON Lines 解析器、动态模拟串口、AI 本地洞察、DeepSeek 浏览器直连、可选后端代理调用和本地兜底 provider
- 硬件/真实 LLM：未连接实物或未观察到真实 DeepSeek 响应时只记录为待测，不伪造结果

## 测试结果

| 项目 | 命令或方式 | 结果 | 备注 |
|---|---|---|---|
| 分支确认 | `git status --short --branch` | 通过 | 当前分支为 `main`；工作树包含既有固件/文档改动以及本次前端重写 |
| 前端全量单测 | `npm --prefix frontend test` | 通过 | 29 项通过，0 项失败；覆盖 parser、analysis、replay source、DeepSeek 直连/proxy 请求、Markdown 渲染、图表选中、回放 UI 和聊天稳定渲染 |
| 前端生产构建 | `npm --prefix frontend run build` | 通过 | TypeScript 与 Vite build 通过；Vite 对 UI/图表栈给出预期的大 chunk 提醒 |
| MONITOR 固件配置 | `cmake --preset MonitorDebug` | 通过 | 生成目录 `build/MonitorDebug` |
| MONITOR 固件构建 | `cmake --build --preset MonitorDebug` | 通过 | 本次输出 `ninja: no work to do`，说明现有构建产物已是最新 |
| SENSOR 固件配置 | `cmake --preset SensorDebug` | 通过 | 生成目录 `build/SensorDebug` |
| SENSOR 固件构建 | `cmake --build --preset SensorDebug` | 通过 | 本次输出 `ninja: no work to do`，说明现有构建产物已是最新 |
| 前端本地服务 | `npm --prefix frontend run dev` | 通过 | Vite dev server 已服务 `http://127.0.0.1:5173/` |
| 动态模拟串口 | 浏览器打开 `http://127.0.0.1:5173/` 并点击“开始模拟” | 通过 | 页面显示“模拟串口”，指标更新，ECharts canvas 渲染，事件栏出现 seq 和循环记录 |
| AI 洞察 UI | 模拟串口运行时观察 AI 区域 | 通过 | 风险等级、主要证据、趋势判断、建议动作随最新 JSON 帧更新 |
| AI 本地对话 | 切换到本地模式后输入“现在安全吗？” | 通过 | 回答引用当前预警状态、MQ135 和 DHT11 证据 |
| DeepSeek 前端直连逻辑 | 默认直连模式下输入“现在安全吗？” | 通过 | 未填写 key 时提示填写；单测验证 key 只放入 `Authorization` header，不进入 JSON body |
| DeepSeek 前端代理逻辑 | URL 配置 `?ai=proxy` 后输入“现在安全吗？” | 通过 | 静态服务无 `/api/ai/chat` 时自动回落到“本地规则兜底”；单测验证请求体包含模型、消息、快照且不包含 API key |
| 断流状态 | 点击“停止模拟”并等待超过 3 秒 | 通过 | Data 显示“数据已超时”，AI 风险变为“节点离线”，事件栏出现“串口数据断流” |
| 趋势图交互 | 点击图表图例/曲线区域 | 通过 | 选中传感器变为 `TEMP 温度`，历史值列表显示最近读数 |
| 响应式 UI | 浏览器视口 1280px 与 390px | 通过 | 无横向溢出；桌面/移动下 AI、聊天、趋势图均可见 |
| 浏览器控制台 | 硬刷新、回放、图表 hover/click、响应式检查 | 通过 | 修正 ECharts 数据形状后无新增 error/warning |
| Web Serial 硬件联调 | 选择板 B USB 转串口 | 待测 | 需要实物连接 |
| DeepSeek V4-flash 真实接口联调 | 页面填写 API key 后提问 | 待测 | 前端已完成直连调用和兜底逻辑；需要用户自行填写 key 并观察真实 DeepSeek 响应 |

## 模拟数据说明

`frontend/fixtures/sample-serial.log` 同时包含 `[MONITOR]` 普通日志和 JSON Lines。`ReplaySerialSource` 会逐行回放该文件，解析器测试会验证普通日志被忽略、有效 JSON 被接收、非法 JSON 和缺字段会被报告为错误。

## 硬件记录

当前未写入任何硬件或真实 DeepSeek 通过结论。只有实际连接板 B、选择 USB 转串口并观察到页面实时刷新后，才能把硬件联调结果改为通过；只有页面填写 API key 并观察到真实 DeepSeek 响应后，才能把真实接口联调结果改为通过。

## 执行时间

- 记录日期：2026-06-08
- 环境：Windows PowerShell，Node `v24.12.0`，Python `3.13.12`
