#pragma once

#include "SensorFrame.hpp"

#include <stdint.h>

namespace app {

class FrameCodec
{
public:
  static constexpr uint8_t head0 = 0xAAu;
  static constexpr uint8_t head1 = 0x55u;
  static constexpr uint8_t version = 2u;
  static constexpr uint8_t payload_len = 18u;
  static constexpr uint8_t total_len = 2u + 1u + payload_len + 1u;

  static uint8_t checksum(const uint8_t *data, uint8_t len);
  static uint8_t encode(const SensorFrame &frame, uint8_t out[total_len]);
  static bool decode(const uint8_t in[total_len], SensorFrame &frame);
};

}  // namespace app
