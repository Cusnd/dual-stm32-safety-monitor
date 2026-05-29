# 开发板与芯片说明清单

[返回中文 README](../README.zh-CN.md) | [硬件接线说明](../WIRING.md) | [模块说明清单](MODULE_REFERENCE.zh-CN.md)

本文档专门说明本项目使用的芯片、核心板和扩展板。它回答三个问题：

1. 本项目用的芯片和开发板是什么。
2. 板级电路和芯片外设为什么能支撑当前设计。
3. 关键引脚在项目中如何分配，哪些引脚不能随意复用。

资料依据包括野火小智 STM32F103C8T6 双 USB 核心板规格书、核心板原理图、核心板引脚分配表、野火小智 STM32F103C8T6 扩展板规格书和原理图，以及 ST STM32F103x8 数据手册与 STM32F10x 参考手册。

## 1. 总览清单

| 类别 | 名称 | 本项目用途 | 关键点 |
|---|---|---|---|
| 主控芯片 | STM32F103C8T6 | 两块板都使用同一 MCU | Cortex-M3，72 MHz，64 KB Flash，20 KB SRAM，LQFP48 |
| 核心板 | 野火小智 STM32F103C8T6 双 USB 核心板 | 板 A 采集节点、板 B 显示报警节点 | 板载 CH340C、SWD、RGB LED、K1/K2、HSE 8 MHz |
| 扩展板 | 野火小智 STM32F103C8T6 扩展板 | 可选，用于直插或转接部分模块 | OLED 可直插；部分模块必须用杜邦线按项目引脚接 |
| 调试下载 | CH340C USB 转串口、SWD | 串口日志、ISP 下载、SWD 下载调试 | `PA9/PA10` 保留给 USART1 调试，`PA13/PA14` 保留给 SWD |
| 双板通信 | USART3 | 板 A 向板 B 发送传感器数据帧 | `PB10/PB11` 交叉连接，115200 8N1 |

## 2. STM32F103C8T6 芯片说明

### 2.1 芯片介绍清单

| 项目 | 说明 | 本项目使用方式 |
|---|---|---|
| 内核 | ARM Cortex-M3 | 运行主循环、串口中断、DWT 微秒延时 |
| 主频 | 最高 72 MHz | 使用外部 8 MHz HSE，经 PLL 倍频到 72 MHz |
| Flash | 64 KB | 两个固件均能放入，采集节点约 18%，显示节点约 28% |
| SRAM | 20 KB | 两个固件 RAM 使用约 10% |
| 工作电压 | 数字电源 2.0-3.6 V；使用 ADC 时 VDDA 至少 2.4 V | 核心板提供 3.3 V，满足 MCU 和 ADC 要求 |
| ADC | 12 位 ADC，支持多通道 | `PA4/ADC1_CH4` 接 MQ135，`PA5/ADC1_CH5` 接 MQ2 |
| USART | 支持 USART1、USART2、USART3 | USART1 做 USB 调试日志，USART3 做双板通信 |
| SPI | 支持 SPI1、SPI2 | 可选 W25Q64 使用 SPI2 |
| GPIO | 支持输入、输出、复用、模拟输入 | 传感器、OLED、蜂鸣器、RGB LED、按键都走 GPIO |
| 调试 | SWD/JTAG | 项目保留 `PA13/PA14` 给 SWD |

### 2.2 时钟设计说明

核心板提供 8 MHz 外部高速晶振 HSE。固件在 `SystemClock_Config()` 中配置：

| 时钟 | 配置 | 作用 |
|---|---|---|
| HSE | 8 MHz | 主 PLL 输入 |
| PLL | 8 MHz x 9 | 得到 72 MHz SYSCLK |
| SYSCLK | 72 MHz | Cortex-M3 内核和大多数外设时钟基准 |
| APB1 | 36 MHz | USART3、SPI2 使用的低速总线 |
| APB2 | 72 MHz | GPIO、ADC1、USART1 等外设总线 |
| ADC 时钟 | APB2 / 6 = 12 MHz | 低于 STM32F1 ADC 14 MHz 上限 |

这套配置与芯片手册、核心板 8 MHz 晶振和项目串口/ADC 时序一致。

### 2.3 芯片级引脚能力说明

| 引脚 | 芯片功能 | 本项目用途 | 说明 |
|---|---|---|---|
| `PA4` | GPIO / ADC1_CH4 / SPI1_NSS | MQ135 AO | 配置为模拟输入，不能接超过 3.3 V 的模拟电压 |
| `PA5` | GPIO / ADC1_CH5 / SPI1_SCK | MQ2 AO | 配置为模拟输入，不能接超过 3.3 V 的模拟电压 |
| `PA9` | USART1_TX | USB 串口调试输出 | 核心板默认通过跳帽连接 CH340C |
| `PA10` | USART1_RX | USB 串口调试输入 | 核心板默认通过跳帽连接 CH340C |
| `PA11/PA12` | USB DM/DP | 保留 | 核心板 USB Device 口使用，项目不复用 |
| `PA13/PA14` | SWDIO/SWCLK | 下载调试 | 不建议外接模块复用 |
| `PB6/PB7` | GPIO / I2C1 相关复用 | OLED 软件 I2C | 项目用软件 I2C，实际按普通 GPIO 开漏输出使用 |
| `PB8` | GPIO / TIM4_CH3 / CAN_RX | 蜂鸣器 SIG | 推挽输出，高电平响 |
| `PB10/PB11` | USART3_TX/RX / I2C2 | 双板 USART3 通信 | 两块板 TX/RX 交叉 |
| `PB12` | GPIO / SPI2_NSS / USART3_CK | 板 A DHT11；板 B W25Q64 CS | 两个固件角色不同，不能在同一块板上混用 |
| `PB13` | GPIO / SPI2_SCK / USART3_CTS | 板 A 火焰 DO；板 B W25Q64 SCK | `PB13` 是 5V tolerant 数字 IO，但模拟输入仍不可 5V |
| `PB14` | GPIO / SPI2_MISO / USART3_RTS | W25Q64 MISO | 显示节点可选 Flash 使用 |
| `PB15` | GPIO / SPI2_MOSI | W25Q64 MOSI | 若使用扩展板，注意扩展板自带 PB15 按键不要按 |
| `PA0` | WKUP / GPIO / ADC | 板载 K1 | 高电平表示按下 |
| `PC13` | GPIO | 板载 K2 | 高电平表示按下 |
| `PA1/PA2/PA3` | GPIO / ADC / TIM2 | 板载 RGB LED | 低电平点亮 |

## 3. 核心板说明

### 3.1 核心板介绍清单

| 模块 | 板上电路 | 项目作用 |
|---|---|---|
| MCU | STM32F103C8T6 LQFP48 | 两个节点主控 |
| 电源 | USB 5 V 输入，LDO 转 3.3 V | 给 MCU、OLED、DHT11、W25Q64 等 3.3 V 模块供电 |
| HSE 晶振 | 8 MHz 外部晶振 | 72 MHz 系统时钟来源 |
| USB 转串口 | CH340C/CH340X，连接 `PA9/PA10` | USART1 调试日志和串口下载 |
| SWD 接口 | `NRST`、`PA13/SWDIO`、`PA14/SWCLK`、3V3、GND | ST-Link 下载和调试 |
| BOOT 配置 | BOOT0、BOOT1 默认下拉 | 默认从内部 Flash 启动 |
| RGB LED | `PA1/PA2/PA3` 低电平点亮 | 显示节点状态灯 |
| 用户按键 | K1=`PA0`，K2=`PC13`，高电平有效 | OLED 翻页、静音、阈值档位切换 |
| 排针 | 引出大部分 MCU IO | 连接传感器、OLED、蜂鸣器、W25Q64 和双板通信线 |

### 3.2 核心板电路说明

核心板电源由 USB 口或外部 5 V 引脚输入，经 3.3 V LDO 给 MCU 和 3.3 V 外设供电。项目中所有 MCU IO 的逻辑基准都是 3.3 V，因此：

- DHT11、OLED、W25Q64 推荐或必须接 3.3 V。
- MQ135/MQ2 模块可以按模块要求接 5 V 给加热丝，但 AO 模拟输出必须保证不超过 3.3 V。
- 火焰模块如果接 5 V，只使用 DO 数字输出时要确认输出电平对 STM32 数字输入安全。

USART1 通过 `PA9/PA10` 接到 CH340C，用于两个节点的调试日志。项目刻意不把 `PA9/PA10` 分配给外部模块，避免和下载、串口日志冲突。

板载 RGB LED 的阳极接 3.3 V，经限流电阻后接到 MCU 引脚。MCU 输出低电平时形成电流回路，所以 RGB LED 是低电平点亮。代码中的 `LED_Set()` 已按这一硬件极性处理。

K1 和 K2 按键按下时把对应 IO 拉高，板上有下拉和消抖电容。代码按高电平有效读取。

### 3.3 核心板保留引脚

| 引脚 | 板上连接 | 项目处理 |
|---|---|---|
| `PA9/PA10` | CH340C USB 转串口 | 保留给 USART1 调试日志 |
| `PA11/PA12` | USB Device DM/DP | 保留，不接外部模块 |
| `PA13/PA14` | SWDIO/SWCLK | 保留给下载调试 |
| `PD0/PD1` | HSE 8 MHz | 不作为普通 IO 使用 |
| `PC14/PC15` | 32.768 kHz 晶振相关位置 | 项目不使用 |
| `PA1/PA2/PA3` | 板载 RGB LED | 只在显示节点作为报警灯 |
| `PA0/PC13` | 板载 K1/K2 | 只在显示节点作为按键 |

## 4. 扩展板说明

### 4.1 扩展板介绍清单

扩展板不是本项目运行的硬性依赖，但可以让模块接线更规整。它的意义是把核心板排针转成常用模块接口，并提供 6-12 V 直流输入、5 V/3.3 V 供电、OLED 插座、通用 IO 插座和通信插座。

| 扩展板接口 | 板上引脚 | 本项目适配情况 |
|---|---|---|
| OLED 插座 | `3V3/GND/PB6/PB7` | 板 B OLED 可直插 |
| 通讯接口 1/2 | `VCC/GND/PB11/PB10/PB8/PB9` | 可用于 USART3 转接，也可引出蜂鸣器 `PB8` |
| 通讯接口 3 | `VCC/GND/PA10/PA9/PA12/PA11` | 不建议用于本项目模块，`PA9/PA10` 已作调试 |
| 通讯接口 4 | `VCC/GND/PB7/PB6/PA7/PA8` | 可用于 OLED/I2C 类模块 |
| 通用 IO1 | `VCC/GND/PA12/PA5` | 可接 MQ2 AO 到 `PA5`，注意线序和电压 |
| 通用 IO2 | `VCC/GND/PA11/PA4` | 可接 MQ135 AO 到 `PA4`，注意线序和电压 |
| 通用 IO3 | `3V3/GND/NC/PB12` | 可接板 A DHT11 |
| 通用 IO4 | `3V3/GND/NC/PA6` | 本项目默认不用 |
| SPI1/W25Q64 直插口 | `VCC/GND/PA4/PA6/PA7/PA5` | 不适合本项目 W25Q64，因为项目用 SPI2 |
| 扩展板按键 | `PB15` | 若板 B 使用 W25Q64 的 SPI2 MOSI，不要按这个按键 |

### 4.2 扩展板电源说明

扩展板可用 6-12 V 直流输入，通过 DCDC 生成 5 V，再生成或分配 3.3 V。使用时注意：

1. 如果只用核心板 USB 供电，扩展板 3.3 V 可用；扩展板 5 V 是否可用取决于自锁开关状态。
2. 如果要从扩展板 5 V 排针给 MQ 模块供电，需要按下扩展板自锁开关。
3. 3.3 V 模块不要误插到 5 V 供电档位，特别是 OLED、DHT11、W25Q64。
4. 两块核心板通信时必须共地，即使它们各自用不同 USB 线供电也要连接 GND。

### 4.3 扩展板与本项目的接线边界

野火官方例程常按单模块演示来分配引脚，本项目是多模块双板系统，因此有几处故意改线：

| 模块 | 野火常见例程/直插思路 | 本项目实际接线 | 原因 |
|---|---|---|---|
| OLED | 有的资料使用 `PB10/PB11`，扩展板 OLED 插座使用 `PB6/PB7` | `PB6/PB7` | `PB10/PB11` 留给双板 USART3 |
| W25Q64 | 扩展板直插 SPI1 `PA4/PA5/PA6/PA7` | SPI2 `PB12/PB13/PB14/PB15` | `PA4/PA5` 已给 MQ135/MQ2 ADC |
| 火焰模块 | 常见例程用 `PA11` 或 `PA5` | `PB13` | `PA11/PA12` 保留 USB，`PA5` 给 MQ2 ADC |
| DHT11 | 常见例程可能用 `PB0` 或 `PB12` | `PB12` | `PB12` 适合单总线，且采集节点不接 W25Q64 |
| 蜂鸣器 | 常见例程可能用 `PA6/PB9` | `PB8` | 显示节点把 `PB8` 作为独立报警输出 |
| MQ135/MQ2 | 两个模块都可能默认 AO 接 `PA4` | MQ135=`PA4`，MQ2=`PA5` | 两路模拟量必须分到两个 ADC 通道 |

## 5. 板 A 与板 B 的开发板级分工

| 节点 | 固件 | 开发板职责 | 使用的核心板资源 |
|---|---|---|---|
| 板 A SENSOR | `Fire_F103_sensor.hex` | 采集 DHT11、MQ135、MQ2、火焰状态，打包后通过 USART3 发送 | ADC1、DWT、USART1、USART3、GPIOB/GPIOA |
| 板 B MONITOR | `Fire_F103_monitor.hex` | 接收数据、刷新 OLED、控制 RGB/蜂鸣器、处理 K1/K2、可选记录 Flash | USART1、USART3、软件 I2C、SPI2、GPIO、SysTick |

两块板使用同一份 `Core/Src/main.c`。在 CLion 中选择 CMake Preset 后，CMake 会通过 `APP_NODE_ROLE` 生成两个固件：

| 预设 | 角色宏 | 输出 |
|---|---|---|
| `SensorDebug` | `APP_NODE_ROLE=1` | `build/SensorDebug/Fire_F103_sensor.hex` |
| `MonitorDebug` | `APP_NODE_ROLE=2` | `build/MonitorDebug/Fire_F103_monitor.hex` |

本项目主开发入口是 CLion + CMake Presets。`MDK-ARM/` 目录不是当前开发流程的入口，只作为参考文件保留。

## 6. 开发板级上电检查清单

| 检查项 | 正确状态 | 错误现象 |
|---|---|---|
| BOOT0/BOOT1 | 默认下拉，从内部 Flash 启动 | 无法运行用户程序，可能进入系统 Bootloader |
| SWD | `PA13/PA14` 未被外接模块占用 | 下载失败或调试连接不稳定 |
| USART1 | `PA9/PA10` 接 CH340C | 串口无日志、下载串口异常 |
| USART3 | 两板 `PB10` 与 `PB11` 交叉连接，GND 共地 | 板 B 显示 `NODE LOST` |
| ADC 输入 | MQ AO 电压不超过 3.3 V | ADC 读数异常，严重时可能损坏 MCU |
| OLED | `PB6/PB7` 接 SCL/SDA，3.3 V 供电 | OLED 无显示或乱码 |
| RGB LED | `PA1/PA2/PA3` 没有外接冲突 | 报警灯颜色错误 |
| 可选 W25Q64 | 3.3 V 供电，SPI2 四线正确 | 无法识别 JEDEC ID，日志不可用 |

## 7. 与代码对应关系

| 硬件层 | 代码位置 | 说明 |
|---|---|---|
| 系统时钟 | `SystemClock_Config()` | HSE 8 MHz 到 72 MHz，APB1 36 MHz，ADC 12 MHz |
| 板载 GPIO | `MX_GPIO_Init()`、`LED_Set()`、`Monitor_UpdateButtons()` | RGB LED、K1/K2 |
| USART1 调试 | `USART1_Init_Custom()`、`_write()` | `printf` 输出到 CH340C |
| USART3 双板通信 | `Node_USART3_Init()`、`USART3_IRQHandler()` | 板 A 发帧，板 B 中断收字节 |
| ADC | `ADC1_Init_Custom()`、`ADC1_ReadChannel()` | MQ135/MQ2 原始 ADC 采样 |
| DHT11 | `DHT11_Read()`、`DHT11_PERIOD_MS` | 单总线时序和安全采样间隔 |
| OLED | `OLED_Init_Custom()`、`OLED_PrintLine()` | 软件 I2C 驱动 SSD1306 |
| W25Q64 | `Flash_Init_Custom()`、`Flash_LogFrame()` | SPI2 读 JEDEC ID、扇区擦除、页写 |
