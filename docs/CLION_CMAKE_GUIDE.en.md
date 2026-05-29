# CLion + CMake Guide

[English README](../README.md) | [中文](CLION_CMAKE_GUIDE.zh-CN.md) | [Project Structure](PROJECT_STRUCTURE.md) | [Board And Chip Reference](BOARD_AND_CHIP_REFERENCE.en.md) | [Module Reference](MODULE_REFERENCE.en.md)

The primary workflow for this repository is **CLion + CMake Presets + Ninja + ARM GCC**. Do not use `MDK-ARM/` as the main project entry point; it is kept only as reference material.

## 1. Environment Checklist

| Tool | Purpose | Requirement |
|---|---|---|
| CLion | Editing, CMake configuration, build entry point | Open the repository root `C:\Users\liuso\Documents\GUC-EMB-Final` |
| CMake | Reads `CMakePresets.json` and configures build folders | Verified working |
| Ninja | Runs the actual build | Verified working |
| ARM GCC | Builds Cortex-M3 bare-metal firmware | `arm-none-eabi-gcc` must be on PATH |
| STM32CubeCLT | Provides ARM GCC, Ninja, flashing/debug tooling | Recommended |
| STM32CubeProgrammer / ST-Link tools | Flashes `.hex` or `.bin` files | Used after CLion builds the firmware |

## 2. Opening The Project In CLion

1. Open CLion.
2. Choose `File -> Open`.
3. Select the repository root:

```text
C:\Users\liuso\Documents\GUC-EMB-Final
```

4. CLion should detect `CMakePresets.json` and create CMake profiles.
5. Select the needed profile in the top-right build selector or under `Settings -> Build, Execution, Deployment -> CMake`.

## 3. CMake Presets

The project does not use two separate firmware projects for Board A and Board B. Instead, CMake passes different role settings into the same source tree.

| CLion / CMake Preset | Role | Output | Flash To |
|---|---|---|---|
| `SensorDebug` | Acquisition node | `build/SensorDebug/Fire_F103_sensor.hex` | Board A |
| `MonitorDebug` | Display/alarm node | `build/MonitorDebug/Fire_F103_monitor.hex` | Board B |
| `Debug` | Default monitor image | `build/Debug/Fire_F103_monitor.hex` | Usually not used for the two-board demo |
| `Release` | Generic release profile | `build/Release/Fire_F103_monitor.hex` | Usually not used for the two-board demo |

Recommended profiles:

```text
SensorDebug
MonitorDebug
```

Using only these two profiles helps avoid flashing the wrong role to a board.

## 4. Building In CLion

Build Board A:

1. Select CMake profile `SensorDebug`.
2. Click Build.
3. Outputs:

```text
build/SensorDebug/Fire_F103_sensor.hex
build/SensorDebug/Fire_F103_sensor.bin
build/SensorDebug/Fire_F103_sensor.elf
```

Build Board B:

1. Select CMake profile `MonitorDebug`.
2. Click Build.
3. Outputs:

```text
build/MonitorDebug/Fire_F103_monitor.hex
build/MonitorDebug/Fire_F103_monitor.bin
build/MonitorDebug/Fire_F103_monitor.elf
```

Command-line equivalent:

```powershell
cmake --preset SensorDebug
cmake --build --preset SensorDebug

cmake --preset MonitorDebug
cmake --build --preset MonitorDebug
```

## 5. CMake File Responsibilities

| File | Purpose |
|---|---|
| `CMakePresets.json` | Provides CLion and command-line profiles such as `SensorDebug` and `MonitorDebug` |
| `CMakeLists.txt` | Defines the firmware target, role selection, output names, and HEX/BIN generation |
| `cmake/gcc-arm-none-eabi.cmake` | Selects ARM GCC, Cortex-M3 flags, and the linker script |
| `cmake/stm32cubemx/CMakeLists.txt` | Includes CubeMX-generated sources, HAL, and CMSIS headers |
| `STM32F103XX_FLASH.ld` | Defines STM32F103C8T6 memory layout: 64 KB Flash and 20 KB RAM |

## 6. How The Role Macro Works

After CLion selects a preset, CMake passes:

| Preset | CMake Variable | C Macro |
|---|---|---|
| `SensorDebug` | `APP_NODE_ROLE=SENSOR` | `APP_NODE_ROLE=1` |
| `MonitorDebug` | `APP_NODE_ROLE=MONITOR` | `APP_NODE_ROLE=2` |

`Core/Src/main.c` uses the macro to select the active main loop:

```c
#if APP_NODE_ROLE == APP_ROLE_SENSOR
  Sensor_App_Run();
#else
  Monitor_App_Run();
#endif
```

If the wrong CLion profile is selected, the wrong firmware role will be generated.

## 7. Flashing Workflow

CLion builds the firmware. Flash the generated artifacts:

| Board | Firmware |
|---|---|
| Board A SENSOR | `build/SensorDebug/Fire_F103_sensor.hex` |
| Board B MONITOR | `build/MonitorDebug/Fire_F103_monitor.hex` |

Possible flashing paths:

- ST-Link + STM32CubeProgrammer flashing `.hex`.
- ST-Link + OpenOCD/GDB debugging `.elf`.
- Serial ISP through the core board CH340C / USART1 download path.

Serial debug settings:

```text
115200 8N1
```

## 8. Common CLion Issues

| Symptom | Likely Cause | Fix |
|---|---|---|
| CLion CMake configure fails and cannot find `arm-none-eabi-gcc` | ARM GCC is not on PATH | Add STM32CubeCLT/ARM GCC to the CLion toolchain or system PATH |
| The generated image behaves as the monitor, not the sensor | `Debug` or `MonitorDebug` was selected | Board A must use `SensorDebug` |
| Board B stays in `NODE LOST` | Board A is not running SENSOR firmware, or USART3 wiring is wrong | Flash Board A with `Fire_F103_sensor.hex`, Board B with `Fire_F103_monitor.hex` |
| OLED stays blank | Firmware was built but not flashed, or OLED wiring/power is wrong | Flash the monitor firmware and check `PB6/PB7/3V3/GND` |
| Manually typed CMake command has a toolchain path issue | Windows path quoting/backslash issue | Prefer CLion presets or `cmake --preset SensorDebug` |

## 9. Keil Is Not Required

The maintained workflow is:

```text
CLion -> CMake Presets -> Ninja -> arm-none-eabi-gcc -> ELF/HEX/BIN
```

`MDK-ARM/` is retained only for compatibility/reference. Day-to-day editing, building, and review should use CLion and CMake.

