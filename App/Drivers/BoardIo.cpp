#include "BoardIo.hpp"

#include "App/BoardPins.hpp"

#include "main.h"

namespace app {

void BoardRgb::set(uint8_t red, uint8_t green, uint8_t blue)
{
  HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, red ? GPIO_PIN_RESET : GPIO_PIN_SET);
  HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, green ? GPIO_PIN_RESET : GPIO_PIN_SET);
  HAL_GPIO_WritePin(LED_B_GPIO_Port, LED_B_Pin, blue ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

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

bool Buttons::key1Pressed() const
{
  return HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin) == GPIO_PIN_SET;
}

bool Buttons::key2Pressed() const
{
  return HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin) == GPIO_PIN_SET;
}

}  // namespace app
