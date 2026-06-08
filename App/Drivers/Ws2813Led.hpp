#pragma once

#include <stdint.h>

namespace app {

class Ws2813Led
{
public:
  void init();
  void setColor(uint8_t red, uint8_t green, uint8_t blue);
  bool ready() const;

private:
  static constexpr uint8_t led_count = 1u;
  static constexpr uint8_t bits_per_led = 24u;
  static constexpr uint8_t reset_slots = 48u;
  static constexpr uint16_t buffer_len = (led_count * bits_per_led) + reset_slots;
  static constexpr uint16_t timer_period = 89u;
  static constexpr uint16_t code0_ccr = 26u;
  static constexpr uint16_t code1_ccr = 52u;

  void fillBuffer(uint8_t red, uint8_t green, uint8_t blue);
  void sendBuffer(uint16_t len);

  uint16_t dma_buffer_[buffer_len];
  uint8_t ready_;
  uint8_t last_r_;
  uint8_t last_g_;
  uint8_t last_b_;
};

}  // namespace app
