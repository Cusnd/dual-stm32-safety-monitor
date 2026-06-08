#include "App/Protocol/FrameCodec.hpp"

#include <assert.h>
#include <stdio.h>
#include <string.h>

namespace {

void assertFrameEqual(const app::SensorFrame &a, const app::SensorFrame &b)
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

app::SensorFrame sampleFrame()
{
  app::SensorFrame frame = {};
  frame.temp = 26u;
  frame.humi = 49u;
  frame.mq135_adc = 2235u;
  frame.mq2_adc = 1810u;
  frame.rain_adc = 1510u;
  frame.therm_adc = 3456u;
  frame.therm_c10 = -87;
  frame.flame = 1u;
  frame.rain_wet = 1u;
  frame.therm_hot = 0u;
  frame.seq = 13u;
  frame.status = app::sensor_status(app::SensorStatus::RainWet);
  return frame;
}

void roundTrip()
{
  const app::SensorFrame frame = sampleFrame();
  uint8_t bytes[app::FrameCodec::total_len];
  app::SensorFrame decoded = {};

  const uint8_t len = app::FrameCodec::encode(frame, bytes);

  assert(len == app::FrameCodec::total_len);
  assert(bytes[0] == app::FrameCodec::head0);
  assert(bytes[1] == app::FrameCodec::head1);
  assert(bytes[2] == app::FrameCodec::payload_len);
  assert(bytes[3] == app::FrameCodec::version);
  assert(bytes[21] == app::FrameCodec::checksum(&bytes[2], 1u + app::FrameCodec::payload_len));
  assert(app::FrameCodec::decode(bytes, decoded));
  assertFrameEqual(frame, decoded);
  assert(decoded.therm_c10 == -87);
}

void rejectsCorruptFrames()
{
  const app::SensorFrame frame = sampleFrame();
  uint8_t bytes[app::FrameCodec::total_len];
  app::SensorFrame decoded = {};

  app::FrameCodec::encode(frame, bytes);

  uint8_t corrupt[app::FrameCodec::total_len];
  memcpy(corrupt, bytes, sizeof(corrupt));
  corrupt[0] = 0x00u;
  assert(!app::FrameCodec::decode(corrupt, decoded));

  memcpy(corrupt, bytes, sizeof(corrupt));
  corrupt[2] = 0x10u;
  assert(!app::FrameCodec::decode(corrupt, decoded));

  memcpy(corrupt, bytes, sizeof(corrupt));
  corrupt[3] = 0x01u;
  assert(!app::FrameCodec::decode(corrupt, decoded));

  memcpy(corrupt, bytes, sizeof(corrupt));
  corrupt[21] ^= 0x01u;
  assert(!app::FrameCodec::decode(corrupt, decoded));
}

}  // namespace

int main()
{
  roundTrip();
  rejectsCorruptFrames();
  puts("protocol_frame_codec_test: ok");
  return 0;
}
