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


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME Fire_F103)

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
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
# This subdirectory contributes CubeMX startup, HAL, CMSIS, and Core/Src files.
# The project-specific logic still lives in Core/Src/main.c.
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

if(APP_NODE_ROLE STREQUAL "SENSOR")
    # Board A image: sensor acquisition node.
    # 板 A 镜像：传感器采集节点。
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE APP_NODE_ROLE=1)
    set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME "Fire_F103_sensor")
elseif(APP_NODE_ROLE STREQUAL "MONITOR")
    # Board B image: OLED display, alarm, buttons, and optional flash logging.
    # 板 B 镜像：OLED 显示、报警、按键和可选 Flash 记录节点。
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE APP_NODE_ROLE=2)
    set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME "Fire_F103_monitor")
else()
    message(FATAL_ERROR "APP_NODE_ROLE must be SENSOR or MONITOR")
endif()

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

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
