#include "App/Protocol/FrameStreamDecoder.hpp"

#include <assert.h>
#include <string.h>

namespace {

app::SensorFrame makeFrame(uint8_t seq)
{
  app::SensorFrame frame = {};
  frame.temp = static_cast<uint8_t>(24u + seq);
  frame.humi = 50u;
  frame.mq135_adc = 2100u;
  frame.mq2_adc = 1200u;
  frame.rain_adc = 900u;
  frame.therm_adc = 2500u;
  frame.therm_c10 = 312;
  frame.flame = 0u;
  frame.rain_wet = 0u;
  frame.therm_hot = 0u;
  frame.seq = seq;
  frame.status = 0u;
  return frame;
}

void assertSameFrame(const app::SensorFrame &a, const app::SensorFrame &b)
{
  assert(a.temp == b.temp);
  assert(a.humi == b.humi);
  assert(a.mq135_adc == b.mq135_adc);
  assert(a.mq2_adc == b.mq2_adc);
  assert(a.rain_adc == b.rain_adc);
  assert(a.therm_adc == b.therm_adc);
  assert(a.therm_c10 == b.therm_c10);
  assert(a.flame == b.flame);
  assert(a.rain_wet == b.rain_wet);
  assert(a.therm_hot == b.therm_hot);
  assert(a.seq == b.seq);
  assert(a.status == b.status);
}

void decodesAfterNoise()
{
  app::FrameStreamDecoder decoder;
  app::SensorFrame decoded = {};
  const app::SensorFrame frame = makeFrame(7u);
  uint8_t bytes[app::FrameCodec::total_len];

  decoder.reset();
  app::FrameCodec::encode(frame, bytes);

  assert(decoder.push(0x00u, decoded) == app::FrameStreamDecoder::Result::NeedMore);
  assert(decoder.push(0x55u, decoded) == app::FrameStreamDecoder::Result::NeedMore);
  for (uint8_t i = 0u; i < app::FrameCodec::total_len - 1u; i++)
  {
    assert(decoder.push(bytes[i], decoded) == app::FrameStreamDecoder::Result::NeedMore);
  }
  assert(decoder.push(bytes[app::FrameCodec::total_len - 1u], decoded) ==
         app::FrameStreamDecoder::Result::FrameReady);
  assertSameFrame(frame, decoded);
}

void resyncsOverlappingHeaders()
{
  app::FrameStreamDecoder decoder;
  app::SensorFrame decoded = {};
  const app::SensorFrame frame = makeFrame(8u);
  uint8_t bytes[app::FrameCodec::total_len];

  decoder.reset();
  app::FrameCodec::encode(frame, bytes);

  assert(decoder.push(app::FrameCodec::head0, decoded) ==
         app::FrameStreamDecoder::Result::NeedMore);
  assert(decoder.push(app::FrameCodec::head0, decoded) ==
         app::FrameStreamDecoder::Result::NeedMore);
  for (uint8_t i = 1u; i < app::FrameCodec::total_len - 1u; i++)
  {
    assert(decoder.push(bytes[i], decoded) == app::FrameStreamDecoder::Result::NeedMore);
  }
  assert(decoder.push(bytes[app::FrameCodec::total_len - 1u], decoded) ==
         app::FrameStreamDecoder::Result::FrameReady);
  assertSameFrame(frame, decoded);
}

void rejectsBadFrameThenRecovers()
{
  app::FrameStreamDecoder decoder;
  app::SensorFrame decoded = {};
  const app::SensorFrame frame = makeFrame(9u);
  uint8_t bytes[app::FrameCodec::total_len];
  uint8_t corrupt[app::FrameCodec::total_len];

  decoder.reset();
  app::FrameCodec::encode(frame, bytes);
  memcpy(corrupt, bytes, sizeof(corrupt));
  corrupt[app::FrameCodec::total_len - 1u] ^= 0x01u;

  for (uint8_t i = 0u; i < app::FrameCodec::total_len - 1u; i++)
  {
    assert(decoder.push(corrupt[i], decoded) == app::FrameStreamDecoder::Result::NeedMore);
  }
  assert(decoder.push(corrupt[app::FrameCodec::total_len - 1u], decoded) ==
         app::FrameStreamDecoder::Result::BadFrame);

  for (uint8_t i = 0u; i < app::FrameCodec::total_len - 1u; i++)
  {
    assert(decoder.push(bytes[i], decoded) == app::FrameStreamDecoder::Result::NeedMore);
  }
  assert(decoder.push(bytes[app::FrameCodec::total_len - 1u], decoded) ==
         app::FrameStreamDecoder::Result::FrameReady);
  assertSameFrame(frame, decoded);
}

void decodesConsecutiveFrames()
{
  app::FrameStreamDecoder decoder;
  app::SensorFrame decoded = {};
  const app::SensorFrame first = makeFrame(1u);
  const app::SensorFrame second = makeFrame(2u);
  uint8_t first_bytes[app::FrameCodec::total_len];
  uint8_t second_bytes[app::FrameCodec::total_len];

  decoder.reset();
  app::FrameCodec::encode(first, first_bytes);
  app::FrameCodec::encode(second, second_bytes);

  for (uint8_t i = 0u; i < app::FrameCodec::total_len - 1u; i++)
  {
    assert(decoder.push(first_bytes[i], decoded) == app::FrameStreamDecoder::Result::NeedMore);
  }
  assert(decoder.push(first_bytes[app::FrameCodec::total_len - 1u], decoded) ==
         app::FrameStreamDecoder::Result::FrameReady);
  assertSameFrame(first, decoded);

  for (uint8_t i = 0u; i < app::FrameCodec::total_len - 1u; i++)
  {
    assert(decoder.push(second_bytes[i], decoded) == app::FrameStreamDecoder::Result::NeedMore);
  }
  assert(decoder.push(second_bytes[app::FrameCodec::total_len - 1u], decoded) ==
         app::FrameStreamDecoder::Result::FrameReady);
  assertSameFrame(second, decoded);
}

}  // namespace

int main()
{
  decodesAfterNoise();
  resyncsOverlappingHeaders();
  rejectsBadFrameThenRecovers();
  decodesConsecutiveFrames();
  return 0;
}
