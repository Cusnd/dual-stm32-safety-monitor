# AGENTS.md

## Project Notes

- This repository is the CLion + CMake workflow for the dual STM32F103C8T6 safety monitor project.
- Local Wildfire STM32F103C8 HAL course/reference material is stored at:

```text
C:\baidunetdiskdownload\野火小智STM32F103C8_HAL库实战资料与课件
```

## Build And Debug

- Use `SensorDebug` for board A and `MonitorDebug` for board B.
- ST-LINK/OpenOCD helpers are configured in `cmake/stlink-stm32f103c8.cfg` and exposed through CMake targets:
  - `stlink_flash`
  - `stlink_server`
  - `stlink_gdb`
- Shared CLion OpenOCD run configurations live under `.idea/runConfigurations/`.
