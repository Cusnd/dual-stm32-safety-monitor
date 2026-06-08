/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.cpp
  * @brief          : C++ entry point for the dual STM32F103C8T6 monitor firmware.
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"
#include "gpio.h"

#include "App/Hal/Hardware.hpp"
#include "App/MonitorNode.hpp"
#include "App/SensorNode.hpp"

#define APP_ROLE_SENSOR   1
#define APP_ROLE_MONITOR  2

#ifndef APP_NODE_ROLE
#define APP_NODE_ROLE APP_ROLE_MONITOR
#endif

extern "C" void SystemClock_Config(void);

namespace {

#if APP_NODE_ROLE == APP_ROLE_SENSOR
app::SensorNode g_sensor_node;
#else
app::MonitorNode g_monitor_node;
#endif

}  // namespace

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();

  app::hal::initDwtDelay();
  app::hal::initDebugUsart1();
  app::hal::initNodeUsart3(APP_NODE_ROLE == APP_ROLE_MONITOR);

#if APP_NODE_ROLE == APP_ROLE_SENSOR
  g_sensor_node.init();
  g_sensor_node.run();
#else
  g_monitor_node.init();
  g_monitor_node.run();
#endif
}

extern "C" void USART3_IRQHandler(void)
{
  app::hal::handleUsart3Irq();
}

extern "C" int __io_putchar(int ch)
{
  return app::hal::writeDebugChar(ch);
}

extern "C" void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

extern "C" void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}

#ifdef USE_FULL_ASSERT
extern "C" void assert_failed(uint8_t *file, uint32_t line)
{
  (void)file;
  (void)line;
}
#endif
