cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME Env-Monitor)

# APP_NODE_ROLE is the switch that turns the shared firmware into one of the
# two boards used in the final project.  The presets select SENSOR or MONITOR,
# and the C source maps those values to APP_NODE_ROLE=1 or APP_NODE_ROLE=2.
# APP_NODE_ROLE 是双节点固件的角色开关：预设选择 SENSOR 或 MONITOR，C 源码
# 再把它们映射成 APP_NODE_ROLE=1 或 APP_NODE_ROLE=2。
set(APP_NODE_ROLE "MONITOR" CACHE STRING "Firmware role: SENSOR or MONITOR")
set_property(CACHE APP_NODE_ROLE PROPERTY STRINGS SENSOR MONITOR)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME} LANGUAGES C CXX ASM)
message("Build type: " ${CMAKE_BUILD_TYPE})

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
# This subdirectory contributes CubeMX startup, HAL, CMSIS, and Core/Src files.
# The project-specific logic is split between Core/Src/main.cpp and App/.
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    App/Hal/Hardware.cpp
    App/Protocol/FrameCodec.cpp
    App/Drivers/BoardIo.cpp
    App/Drivers/Dht11.cpp
    App/Drivers/OledDisplay.cpp
    App/Drivers/W25q64FlashLogger.cpp
    App/Drivers/Ws2813Led.cpp
    App/SensorNode.cpp
    App/MonitorNode.cpp
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

if(APP_NODE_ROLE STREQUAL "SENSOR")
    # Board A image: sensor acquisition node.
    # 板 A 镜像：传感器采集节点。
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE APP_NODE_ROLE=1)
    set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME "Env-Monitor_sensor")
elseif(APP_NODE_ROLE STREQUAL "MONITOR")
    # Board B image: OLED display, alarm, buttons, and optional flash logging.
    # 板 B 镜像：OLED 显示、报警、按键和可选 Flash 记录节点。
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE APP_NODE_ROLE=2)
    set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME "Env-Monitor_monitor")
else()
    message(FATAL_ERROR "APP_NODE_ROLE must be SENSOR or MONITOR")
endif()

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)
list(REMOVE_ITEM CMAKE_CXX_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)

add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    # Keil/ST-Link tools usually accept ELF, HEX, or BIN.  Generating HEX/BIN
    # here makes both role images easy to burn without extra manual commands.
    # Keil/ST-Link 通常可烧录 ELF、HEX 或 BIN；这里自动生成 HEX/BIN，方便
    # 两个节点直接烧录。
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
            $<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/$<TARGET_FILE_BASE_NAME:${CMAKE_PROJECT_NAME}>.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
            $<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/$<TARGET_FILE_BASE_NAME:${CMAKE_PROJECT_NAME}>.bin
    COMMENT "Generating HEX and BIN images"
)

# ST-LINK / OpenOCD helpers.  CLion can use the shared run configurations under
# .idea/runConfigurations, and the same targets are available from the terminal:
#
#   cmake --build --preset SensorStlinkFlash
#   cmake --build --preset MonitorStlinkFlash
#
# For a manual GDB session, run stlink_server in one terminal and stlink_gdb in
# another terminal for the same CMake preset.
set(STLINK_OPENOCD_CONFIG
    "${CMAKE_SOURCE_DIR}/cmake/stlink-stm32f103c8.cfg"
    CACHE FILEPATH "OpenOCD board config for ST-LINK + STM32F103C8T6")
set(STLINK_GDB_PORT "3333" CACHE STRING "OpenOCD GDB port")
set(STLINK_TELNET_PORT "4444" CACHE STRING "OpenOCD telnet port")
set(STLINK_TCL_PORT "6666" CACHE STRING "OpenOCD TCL port")

find_program(OPENOCD_EXECUTABLE
    NAMES openocd openocd.exe
    DOC "OpenOCD executable")
if(NOT OPENOCD_EXECUTABLE)
    set(OPENOCD_EXECUTABLE "openocd" CACHE FILEPATH "OpenOCD executable" FORCE)
endif()

find_program(ARM_NONE_EABI_GDB_EXECUTABLE
    NAMES arm-none-eabi-gdb arm-none-eabi-gdb.exe
    DOC "ARM none-eabi GDB executable")
if(NOT ARM_NONE_EABI_GDB_EXECUTABLE)
    set(ARM_NONE_EABI_GDB_EXECUTABLE "arm-none-eabi-gdb" CACHE FILEPATH
        "ARM none-eabi GDB executable" FORCE)
endif()

add_custom_target(stlink_server
    COMMAND "${OPENOCD_EXECUTABLE}"
            -f "${STLINK_OPENOCD_CONFIG}"
            -c "gdb_port ${STLINK_GDB_PORT}"
            -c "telnet_port ${STLINK_TELNET_PORT}"
            -c "tcl_port ${STLINK_TCL_PORT}"
    COMMENT "Starting OpenOCD ST-LINK debug server on GDB port ${STLINK_GDB_PORT}"
    USES_TERMINAL
)

add_custom_target(stlink_flash
    DEPENDS ${CMAKE_PROJECT_NAME}
    COMMAND "${OPENOCD_EXECUTABLE}"
            -f "${STLINK_OPENOCD_CONFIG}"
            -c "gdb_port ${STLINK_GDB_PORT}"
            -c "telnet_port ${STLINK_TELNET_PORT}"
            -c "tcl_port ${STLINK_TCL_PORT}"
            -c "program {$<TARGET_FILE:${CMAKE_PROJECT_NAME}>} verify reset exit"
    COMMENT "Flashing $<TARGET_FILE_NAME:${CMAKE_PROJECT_NAME}> through ST-LINK"
    USES_TERMINAL
)

add_custom_target(stlink_gdb
    DEPENDS ${CMAKE_PROJECT_NAME}
    COMMAND "${ARM_NONE_EABI_GDB_EXECUTABLE}"
            "$<TARGET_FILE:${CMAKE_PROJECT_NAME}>"
            -ex "target extended-remote localhost:${STLINK_GDB_PORT}"
            -ex "monitor reset halt"
            -ex "load"
            -ex "monitor reset halt"
    COMMENT "Starting GDB for $<TARGET_FILE_NAME:${CMAKE_PROJECT_NAME}>; run stlink_server first"
    USES_TERMINAL
)
