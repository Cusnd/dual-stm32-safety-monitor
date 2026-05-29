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
| `SensorDebug` | 采集节点 | `build/SensorDebug/Fire_F103_sensor.hex` | 板 A |
| `MonitorDebug` | 显示报警节点 | `build/MonitorDebug/Fire_F103_monitor.hex` | 板 B |
| `Debug` | 默认显示节点 | `build/Debug/Fire_F103_monitor.hex` | 一般不用作双板演示 |
| `Release` | 通用发布配置 | `build/Release/Fire_F103_monitor.hex` | 一般不用作双板演示 |

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
build/SensorDebug/Fire_F103_sensor.hex
build/SensorDebug/Fire_F103_sensor.bin
build/SensorDebug/Fire_F103_sensor.elf
```

构建板 B：

1. 选择 CMake profile：`MonitorDebug`。
2. 点击 Build。
3. 输出文件位于：

```text
build/MonitorDebug/Fire_F103_monitor.hex
build/MonitorDebug/Fire_F103_monitor.bin
build/MonitorDebug/Fire_F103_monitor.elf
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

`Core/Src/main.c` 根据这个宏选择运行哪个主循环：

```c
#if APP_NODE_ROLE == APP_ROLE_SENSOR
  Sensor_App_Run();
#else
  Monitor_App_Run();
#endif
```

所以在 CLion 里选错 profile，就会生成错节点固件。

## 7. 烧录流程

CLion 主要负责构建。烧录时使用构建产物：

| 开发板 | 烧录文件 |
|---|---|
| 板 A SENSOR | `build/SensorDebug/Fire_F103_sensor.hex` |
| 板 B MONITOR | `build/MonitorDebug/Fire_F103_monitor.hex` |

可用方式：

- ST-Link + STM32CubeProgrammer 烧录 `.hex`。
- ST-Link + OpenOCD/GDB 调试 `.elf`。
- 串口 ISP 下载时，使用核心板 CH340C 对应的 USART1 下载链路。

烧录后串口调试参数为：

```text
115200 8N1
```

## 8. CLion 常见问题

| 现象 | 原因 | 处理 |
|---|---|---|
| CLion 配置 CMake 失败，提示找不到 `arm-none-eabi-gcc` | ARM GCC 不在 PATH | 在 CLion Toolchain 或系统 PATH 中加入 STM32CubeCLT/ARM GCC 路径 |
| 生成的是显示节点，不是采集节点 | 选了 `Debug` 或 `MonitorDebug` | 板 A 必须选 `SensorDebug` |
| 板 B 一直 `NODE LOST` | 板 A 没运行 SENSOR 固件，或 USART3 接线错误 | 确认板 A 烧 `Fire_F103_sensor.hex`，板 B 烧 `Fire_F103_monitor.hex` |
| OLED 不显示 | 只构建了固件但未烧录，或 OLED 接线/供电错误 | 确认烧录 Monitor 固件，并检查 `PB6/PB7/3V3/GND` |
| 命令行手写 CMake 时工具链路径异常 | Windows 路径和反斜杠转义问题 | 优先用 CLion preset 或 `cmake --preset SensorDebug` |

## 9. 不需要使用 Keil

本项目当前维护和验证的主路径是：

```text
CLion -> CMake Presets -> Ninja -> arm-none-eabi-gcc -> ELF/HEX/BIN
```

`MDK-ARM/` 目录只是为了保留兼容资料和参考，不是你当前开发流程的入口。日常修改、构建、审查都以 CLion 和 CMake 为准。
