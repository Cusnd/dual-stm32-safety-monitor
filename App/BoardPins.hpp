#pragma once

#include "main.h"

#include <stdint.h>

namespace app::pins {

inline GPIO_TypeDef * const dht11_port = GPIOB;
constexpr uint16_t dht11_pin = GPIO_PIN_12;

inline GPIO_TypeDef * const flame_port = GPIOB;
constexpr uint16_t flame_pin = GPIO_PIN_13;

inline GPIO_TypeDef * const therm_do_port = GPIOB;
constexpr uint16_t therm_do_pin = GPIO_PIN_9;

inline GPIO_TypeDef * const oled_port = GPIOB;
constexpr uint16_t oled_scl_pin = GPIO_PIN_6;
constexpr uint16_t oled_sda_pin = GPIO_PIN_7;

inline GPIO_TypeDef * const buzzer_port = GPIOB;
constexpr uint16_t buzzer_pin = GPIO_PIN_8;

inline GPIO_TypeDef * const flash_cs_port = GPIOB;
constexpr uint16_t flash_cs_pin = GPIO_PIN_12;

inline GPIO_TypeDef * const external_rgb_port = GPIOA;
constexpr uint16_t external_rgb_pin = GPIO_PIN_6;

}  // namespace app::pins
