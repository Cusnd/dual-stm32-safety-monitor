# Frontend Dashboard Test Record

[中文](FRONTEND_TEST_RECORD.zh-CN.md) | [Back to README](../README.md)

## Scope

- Branch: `dev/frontend`
- Firmware: `MonitorDebug`, `SensorDebug`
- Frontend: static Web Serial page, JSON Lines parser, replay serial source, local AI insights, DeepSeek browser-direct calls, optional backend proxy calls, and local fallback provider
- Hardware / real LLM: marked pending unless a real board or real DeepSeek response is connected and observed

## Results

| Item | Command or Method | Result | Notes |
|---|---|---|---|
| Branch check | `git status --short --branch` | Passed | Current branch is `dev/frontend`; `project-introduction.html` remains untracked |
| Frontend full test | `npm --prefix frontend test` | Passed | 23 passed, 0 failed; covers parser, analysis, replay source, DeepSeek direct requests, proxy requests, plain-text replies, and local fallback |
| MONITOR configure | `cmake --preset MonitorDebug` | Passed | Generated `build/MonitorDebug` |
| MONITOR build | `cmake --build --preset MonitorDebug` | Passed | This run printed `ninja: no work to do`, meaning the build output was current |
| SENSOR configure | `cmake --preset SensorDebug` | Passed | Generated `build/SensorDebug` |
| SENSOR build | `cmake --build --preset SensorDebug` | Passed | This run printed `ninja: no work to do`, meaning the build output was current |
| Frontend local server | `python -m http.server 5173 -d frontend` | Passed | Local server started and used for browser verification |
| Replay serial UI | Open `http://localhost:5173` and press "Start replay" | Passed | Page showed replay serial mode, button changed to stop, and event log showed seq and loop rows |
| AI insights UI | Observe AI panel while replay runs | Passed | Risk level, evidence, trend, and action updated from the latest JSON frame |
| Local AI chat | Switch to local mode and ask "现在安全吗？" | Passed | Answer referenced the current warning state, MQ135, and DHT11 evidence |
| DeepSeek frontend direct logic | Ask "现在安全吗？" in default direct mode | Passed | Without a key, the UI asks for one; tests verify the key is sent only in the `Authorization` header and not in the JSON body |
| DeepSeek frontend proxy logic | Configure `?ai=proxy` and ask "现在安全吗？" | Passed | With static hosting and no `/api/ai/chat`, the UI fell back to "Local fallback"; tests verify the request body includes model, messages, snapshot, and no API key |
| Stale state | Press "Stop replay" and wait more than 3 seconds | Passed | Data showed stale, AI risk changed to node lost, and event log showed stream stopped |
| Responsive UI | Browser viewports 1280px and 390px | Passed | No horizontal overflow; AI, chat, and chart areas were visible on desktop and mobile |
| Browser console | Read error/warning logs | Passed | 0 error/warning logs |
| Web Serial hardware check | Select Board B USB-UART serial port | Pending | Requires physical board |
| DeepSeek V4-flash real API check | Enter an API key in the page and ask a question | Pending | Frontend direct call and fallback logic are complete; requires the user-entered key and an observed real DeepSeek response |

## Simulated Data

`frontend/fixtures/sample-serial.log` contains both ordinary `[MONITOR]` logs and JSON Lines. `ReplaySerialSource` streams it line by line. The parser test verifies that ordinary logs are ignored, valid JSON is accepted, and malformed or incomplete JSON is reported as an error.

## Hardware Record

No hardware or real DeepSeek pass result is recorded yet. The hardware check can only be marked passed after Board B is connected, the USB-UART serial port is selected, and live browser updates are observed. The real API check can only be marked passed after an API key is entered in the page and a real DeepSeek response is observed.

## Execution Time

- Record date: 2026-06-08
- Environment: Windows PowerShell, Node `v24.12.0`, Python `3.13.12`
