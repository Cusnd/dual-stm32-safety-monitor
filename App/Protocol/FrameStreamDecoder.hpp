#pragma once

#include "FrameCodec.hpp"
#include "SensorFrame.hpp"

#include <stdint.h>

namespace app {

class FrameStreamDecoder
{
public:
  enum class Result : uint8_t
  {
    NeedMore,
    FrameReady,
    BadFrame,
  };

  void reset();
  Result push(uint8_t byte, SensorFrame &frame);

private:
  uint8_t buffer_[FrameCodec::total_len];
  uint8_t pos_;
};

}  // namespace app
