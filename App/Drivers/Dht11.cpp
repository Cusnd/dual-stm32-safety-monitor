#include "Dht11.hpp"

#include "App/BoardPins.hpp"
#include "App/Hal/Hardware.hpp"

#include "main.h"

namespace app {

void Dht11::initGpio()
{
  GPIO_InitTypeDef gpio = {0};
  gpio.Pin = pins::dht11_pin;
  gpio.Mode = GPIO_MODE_OUTPUT_OD;
  gpio.Pull = GPIO_PULLUP;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(pins::dht11_port, &gpio);
  HAL_GPIO_WritePin(pins::dht11_port, pins::dht11_pin, GPIO_PIN_SET);
}

void Dht11::setOutput()
{
  GPIO_InitTypeDef gpio = {0};
  gpio.Pin = pins::dht11_pin;
  gpio.Mode = GPIO_MODE_OUTPUT_OD;
  gpio.Pull = GPIO_PULLUP;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(pins::dht11_port, &gpio);
}

void Dht11::setInput()
{
  GPIO_InitTypeDef gpio = {0};
  gpio.Pin = pins::dht11_pin;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(pins::dht11_port, &gpio);
}

bool Dht11::waitLevel(int level, uint32_t timeout_us)
{
  const auto expected = static_cast<GPIO_PinState>(level);
  const uint32_t ticks = timeout_us * (SystemCoreClock / 1000000u);
  const uint32_t start = DWT->CYCCNT;

  while (HAL_GPIO_ReadPin(pins::dht11_port, pins::dht11_pin) != expected)
  {
    if (static_cast<uint32_t>(DWT->CYCCNT - start) > ticks)
    {
      return false;
    }
  }
  return true;
}

bool Dht11::read(uint8_t &temp, uint8_t &humi)
{
  uint8_t data[5] = {0u, 0u, 0u, 0u, 0u};

  setOutput();
  HAL_GPIO_WritePin(pins::dht11_port, pins::dht11_pin, GPIO_PIN_RESET);
  HAL_Delay(20u);
  HAL_GPIO_WritePin(pins::dht11_port, pins::dht11_pin, GPIO_PIN_SET);
  hal::delayUs(30u);
  setInput();

  if (!waitLevel(GPIO_PIN_RESET, 100u) ||
      !waitLevel(GPIO_PIN_SET, 100u) ||
      !waitLevel(GPIO_PIN_RESET, 100u))
  {
    return false;
  }

  for (uint8_t i = 0u; i < 40u; i++)
  {
    if (!waitLevel(GPIO_PIN_SET, 70u))
    {
      return false;
    }

    hal::delayUs(40u);
    if (HAL_GPIO_ReadPin(pins::dht11_port, pins::dht11_pin) == GPIO_PIN_SET)
    {
      data[i / 8u] |= static_cast<uint8_t>(1u << (7u - (i % 8u)));
      if (!waitLevel(GPIO_PIN_RESET, 80u))
      {
        return false;
      }
    }
  }

  if (static_cast<uint8_t>(data[0] + data[1] + data[2] + data[3]) != data[4])
  {
    return false;
  }

  humi = data[0];
  temp = data[2];
  return true;
}

}  // namespace app
