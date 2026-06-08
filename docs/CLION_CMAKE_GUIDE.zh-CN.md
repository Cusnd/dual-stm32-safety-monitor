# CLion + CMake 使用说明

[返回中文 README](../README.zh-CN.md) | [English](CLION_CMAKE_GUIDE.en.md) | [项目结构](PROJECT_STRUCTURE.md) | [开发板与芯片](BOARD_AND_CHIP_REFERENCE.zh-CN.md) | [模块清单](MODULE_REFERENCE.zh-CN.md)

本仓库的主开发方式是 **CLion + CMake Presets + Ninja + ARM GCC**。不要把 `MDK-ARM/` 当作主工程；它只是仓库里保留的参考文件。

## 1. 环境清单

| 工具 | 用途 | 要求 |
|---|---|---|
| CLion | 代码编辑、CMake 配置、构建入口 | 打开仓库根目录 `C:\Users\liuso\Documents\GUC-EMB-Final` |
| CMake | 读取 `CMakePresets.json` 并生成构建目录 | 已验证可用 |
| Ninja | 执行实际编译 | 已验证可用 |
| ARM GCC | 编译 Cortex-M3 裸机固件 | `arm-none-eabi-gcc` 需要在 PATH 中 |
| STM32CubeCLT | 提供 ARM GCC、Ninja、烧录/调试工具链 | 推荐安装 |
| STM32CubeProgrammer / ST-Link 工具 | 烧录 `.hex` 或 `.bin` | 用于把 CLion 编译出的固件写入开发板 |
| OpenOCD | ST-LINK SWD 调试服务器 | `openocd` 需要在 PATH 中，或在 CLion Embedded Development 设置中配置 |
| ARM GDB | 连接 OpenOCD 调试 `.elf` | `arm-none-eabi-gdb` 需要在 PATH 中 |

## 2. 在 CLion 中打开项目

1. 打开 CLion。
2. 选择 `File -> Open`。
3. 选择仓库根目录：

```text
C:\Users\liuso\Documents\GUC-EMB-Final
```

4. CLion 识别到 `CMakePresets.json` 后，会自动创建 CMake profile。
5. 在右上角或 `Settings -> Build, Execution, Deployment -> CMake` 中选择需要的 profile。

## 3. CMake Preset 选择

本项目不是用两个工程文件区分板 A 和板 B，而是用 CMake preset 给同一份源码传入不同角色。

| CLion / CMake Preset | 角色 | 生成文件 | 烧录到 |
|---|---|---|---|
| `SensorDebug` | 采集节点 | `build/SensorDebug/Env-Monitor_sensor.hex` | 板 A |
| `MonitorDebug` | 显示报警节点 | `build/MonitorDebug/Env-Monitor_monitor.hex` | 板 B |
| `Debug` | 默认显示节点 | `build/Debug/Env-Monitor_monitor.hex` | 一般不用作双板演示 |
| `Release` | 通用发布配置 | `build/Release/Env-Monitor_monitor.hex` | 一般不用作双板演示 |

推荐只用：

```text
SensorDebug
MonitorDebug
```

这样不会烧错节点角色。

## 4. 在 CLion 中构建

构建板 A：

1. 选择 CMake profile：`SensorDebug`。
2. 点击 Build。
3. 输出文件位于：

```text
build/SensorDebug/Env-Monitor_sensor.hex
build/SensorDebug/Env-Monitor_sensor.bin
build/SensorDebug/Env-Monitor_sensor.elf
```

构建板 B：

1. 选择 CMake profile：`MonitorDebug`。
2. 点击 Build。
3. 输出文件位于：

```text
build/MonitorDebug/Env-Monitor_monitor.hex
build/MonitorDebug/Env-Monitor_monitor.bin
build/MonitorDebug/Env-Monitor_monitor.elf
```

命令行等价操作是：

```powershell
cmake --preset SensorDebug
cmake --build --preset SensorDebug

cmake --preset MonitorDebug
cmake --build --preset MonitorDebug
```

## 5. CMake 文件分工

| 文件 | 作用 |
|---|---|
| `CMakePresets.json` | 给 CLion 和命令行提供 `SensorDebug`、`MonitorDebug` 等构建配置 |
| `CMakeLists.txt` | 定义固件目标、角色选择、输出文件名、HEX/BIN 生成 |
| `cmake/gcc-arm-none-eabi.cmake` | 指定 ARM GCC、Cortex-M3 编译参数、链接脚本 |
| `cmake/stm32cubemx/CMakeLists.txt` | 引入 CubeMX 生成源码、HAL 和 CMSIS 头文件 |
| `STM32F103XX_FLASH.ld` | 定义 STM32F103C8T6 的 64 KB Flash 和 20 KB RAM |

## 6. 角色宏如何生效

CLion 选择 preset 后，CMake 会传入：

| Preset | CMake 变量 | C 编译宏 |
|---|---|---|
| `SensorDebug` | `APP_NODE_ROLE=SENSOR` | `APP_NODE_ROLE=1` |
| `MonitorDebug` | `APP_NODE_ROLE=MONITOR` | `APP_NODE_ROLE=2` |

`Core/Src/main.cpp` 根据这个宏选择运行哪个 C++ 节点对象：

```cpp
#if APP_NODE_ROLE == APP_ROLE_SENSOR
  g_sensor_node.init();
  g_sensor_node.run();
#else
  g_monitor_node.init();
  g_monitor_node.run();
#endif
```

所以在 CLion 里选错 profile，就会生成错节点固件。

## 7. 烧录流程

CLion 主要负责构建。烧录时使用构建产物：

| 开发板 | 烧录文件 |
|---|---|
| 板 A SENSOR | `build/SensorDebug/Env-Monitor_sensor.hex` |
| 板 B MONITOR | `build/MonitorDebug/Env-Monitor_monitor.hex` |

可用方式：

- ST-Link + STM32CubeProgrammer 烧录 `.hex`。
- ST-Link + OpenOCD/GDB 调试 `.elf`。
- 串口 ISP 下载时，使用目标板 USB 转串口对应的 USART1 下载链路。

本仓库已经内置 ST-LINK/OpenOCD 配置：

| 文件 / 目标 | 用途 |
|---|---|
| `cmake/stlink-stm32f103c8.cfg` | OpenOCD 的 ST-LINK + STM32F103C8T6 板级配置 |
| `stlink_flash` | 构建当前 preset 的固件并通过 ST-LINK 烧录 |
| `stlink_server` | 启动 OpenOCD，开放 GDB 端口 `3333` |
| `stlink_gdb` | 启动 `arm-none-eabi-gdb` 并连接 `localhost:3333` |

命令行一键烧录：

```powershell
cmake --preset SensorDebug
cmake --build --preset SensorStlinkFlash

cmake --preset MonitorDebug
cmake --build --preset MonitorStlinkFlash
```

命令行手动调试时，先开 OpenOCD，再开 GDB：

```powershell
cmake --build --preset SensorStlinkServer
# 另开一个终端
cmake --build --preset SensorStlinkGdb
```

板 B 把上面命令中的 `Sensor...` 换成 `Monitor...`。

CLion 中可以直接使用共享运行配置：

| CLion Run Configuration | 固件 |
|---|---|
| `STLINK OpenOCD Sensor` | `build/SensorDebug/Env-Monitor_sensor.elf` |
| `STLINK OpenOCD Monitor` | `build/MonitorDebug/Env-Monitor_monitor.elf` |

如果 CLion 没自动识别共享配置，可手动新建 `OpenOCD Download & Run`，参数填：

```text
Board config file: $PROJECT_DIR$/cmake/stlink-stm32f103c8.cfg
GDB port: 3333
Download: Always
Reset: Halt
Sensor executable: $PROJECT_DIR$/build/SensorDebug/Env-Monitor_sensor.elf
Monitor executable: $PROJECT_DIR$/build/MonitorDebug/Env-Monitor_monitor.elf
```

烧录后串口调试参数为：

```text
115200 8N1
```

## 8. ST-LINK 接线

| 现象 | 原因 | 处理 |
|---|---|---|
| OpenOCD 提示找不到目标 MCU | SWD 接线或供电异常 | 检查 `3V3/GND/PA13/PA14/NRST`，并确认开发板已上电 |
| GDB 连接不上 `localhost:3333` | OpenOCD 未启动或端口被占用 | 先运行 `stlink_server`，或关闭占用 `3333` 的程序 |
| 烧录后角色不对 | 使用了错误 preset | 板 A 用 `SensorStlinkFlash`，板 B 用 `MonitorStlinkFlash` |

ST-LINK SWD 接线：

```text
ST-LINK 3V3  -> Board 3V3
ST-LINK GND  -> Board GND
ST-LINK SWDIO -> PA13/SWDIO
ST-LINK SWCLK -> PA14/SWCLK
ST-LINK NRST -> NRST
```

`PA13/PA14` 是 SWD 引脚，不要复用给外设。

## 9. CLion 常见问题

| 现象 | 原因 | 处理 |
|---|---|---|
| CLion 配置 CMake 失败，提示找不到 `arm-none-eabi-gcc` | ARM GCC 不在 PATH | 在 CLion Toolchain 或系统 PATH 中加入 STM32CubeCLT/ARM GCC 路径 |
| CLion 运行 `STLINK OpenOCD ...` 时提示 OpenOCD 位置未设置 | Embedded Development 中没有配置 OpenOCD | `Settings -> Build, Execution, Deployment -> Embedded Development` 设置 OpenOCD 路径 |
| 生成的是显示节点，不是采集节点 | 选了 `Debug` 或 `MonitorDebug` | 板 A 必须选 `SensorDebug` |
| 板 B 一直 `NODE LOST` | 板 A 没运行 SENSOR 固件，或 USART3 接线错误 | 确认板 A 烧 `Env-Monitor_sensor.hex`，板 B 烧 `Env-Monitor_monitor.hex` |
| OLED 不显示 | 只构建了固件但未烧录，或 OLED 接线/供电错误 | 确认烧录 Monitor 固件，并检查 `PB6/PB7/3V3/GND` |
| 命令行手写 CMake 时工具链路径异常 | Windows 路径和反斜杠转义问题 | 优先用 CLion preset 或 `cmake --preset SensorDebug` |

## 10. 不需要使用 Keil

本项目当前维护和验证的主路径是：

```text
CLion -> CMake Presets -> Ninja -> arm-none-eabi-gcc -> ELF/HEX/BIN
```

`MDK-ARM/` 目录只是为了保留兼容资料和参考，不是你当前开发流程的入口。日常修改、构建、审查都以 CLion 和 CMake 为准。
