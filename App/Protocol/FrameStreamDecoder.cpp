#include "FrameStreamDecoder.hpp"

namespace app {

void FrameStreamDecoder::reset()
{
  pos_ = 0u;
}

FrameStreamDecoder::Result FrameStreamDecoder::push(uint8_t byte, SensorFrame &frame)
{
  if (pos_ == 0u)
  {
    if (byte != FrameCodec::head0)
    {
      return Result::NeedMore;
    }
  }
  else if ((pos_ == 1u) && (byte != FrameCodec::head1))
  {
    pos_ = (byte == FrameCodec::head0) ? 1u : 0u;
    if (pos_ == 1u)
    {
      buffer_[0] = FrameCodec::head0;
    }
    return Result::NeedMore;
  }

  buffer_[pos_++] = byte;
  if (pos_ < FrameCodec::total_len)
  {
    return Result::NeedMore;
  }

  pos_ = 0u;
  return FrameCodec::decode(buffer_, frame) ? Result::FrameReady : Result::BadFrame;
}

}  // namespace app
