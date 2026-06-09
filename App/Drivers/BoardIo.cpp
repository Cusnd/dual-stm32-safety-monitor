#include "BoardIo.hpp"

#include "App/BoardPins.hpp"

#include "main.h"

namespace app {

void Buzzer::init()
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOB_CLK_ENABLE();

  gpio.Pin = pins::buzzer_pin;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(pins::buzzer_port, &gpio);
  set(false);
}

void Buzzer::set(bool on)
{
  HAL_GPIO_WritePin(pins::buzzer_port, pins::buzzer_pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void Buttons::init()
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOB_CLK_ENABLE();

  gpio.Pin = pins::threshold_select_pin | pins::threshold_level_pin;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &gpio);
}

bool Buttons::key1Pressed() const
{
  return HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin) == GPIO_PIN_SET;
}

bool Buttons::key2Pressed() const
{
  return HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin) == GPIO_PIN_SET;
}

bool Buttons::thresholdSelectPressed() const
{
  return HAL_GPIO_ReadPin(pins::threshold_select_port, pins::threshold_select_pin) == GPIO_PIN_RESET;
}

bool Buttons::thresholdLevelPressed() const
{
  return HAL_GPIO_ReadPin(pins::threshold_level_port, pins::threshold_level_pin) == GPIO_PIN_RESET;
}

}  // namespace app
