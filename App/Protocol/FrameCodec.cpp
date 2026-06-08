#include "FrameCodec.hpp"

namespace app {
namespace {

enum FrameOffset : uint8_t
{
  offset_head0 = 0u,
  offset_head1 = 1u,
  offset_len = 2u,
  offset_version = 3u,
  offset_temp = 4u,
  offset_humi = 5u,
  offset_mq135 = 6u,
  offset_mq2 = 8u,
  offset_rain = 10u,
  offset_therm_adc = 12u,
  offset_therm_c10 = 14u,
  offset_flame = 16u,
  offset_rain_wet = 17u,
  offset_therm_hot = 18u,
  offset_seq = 19u,
  offset_status = 20u,
  offset_checksum = 21u,
};

uint16_t readU16(const uint8_t *data, uint8_t offset)
{
  return static_cast<uint16_t>((static_cast<uint16_t>(data[offset]) << 8) |
                               data[offset + 1u]);
}

void writeU16(uint8_t *data, uint8_t offset, uint16_t value)
{
  data[offset] = static_cast<uint8_t>(value >> 8);
  data[offset + 1u] = static_cast<uint8_t>(value & 0xFFu);
}

}  // namespace

uint8_t FrameCodec::checksum(const uint8_t *data, uint8_t len)
{
  uint8_t sum = 0u;
  for (uint8_t i = 0u; i < len; i++)
  {
    sum = static_cast<uint8_t>(sum + data[i]);
  }
  return sum;
}

uint8_t FrameCodec::encode(const SensorFrame &frame, uint8_t out[total_len])
{
  out[offset_head0] = head0;
  out[offset_head1] = head1;
  out[offset_len] = payload_len;
  out[offset_version] = version;
  out[offset_temp] = frame.temp;
  out[offset_humi] = frame.humi;
  writeU16(out, offset_mq135, frame.mq135_adc);
  writeU16(out, offset_mq2, frame.mq2_adc);
  writeU16(out, offset_rain, frame.rain_adc);
  writeU16(out, offset_therm_adc, frame.therm_adc);
  writeU16(out, offset_therm_c10, static_cast<uint16_t>(frame.therm_c10));
  out[offset_flame] = frame.flame;
  out[offset_rain_wet] = frame.rain_wet;
  out[offset_therm_hot] = frame.therm_hot;
  out[offset_seq] = frame.seq;
  out[offset_status] = frame.status;
  out[offset_checksum] = checksum(&out[offset_len], static_cast<uint8_t>(1u + payload_len));
  return total_len;
}

bool FrameCodec::decode(const uint8_t in[total_len], SensorFrame &frame)
{
  if ((in[offset_head0] != head0) || (in[offset_head1] != head1) ||
      (in[offset_len] != payload_len) || (in[offset_version] != version))
  {
    return false;
  }

  if (checksum(&in[offset_len], static_cast<uint8_t>(1u + payload_len)) !=
      in[offset_checksum])
  {
    return false;
  }

  frame.temp = in[offset_temp];
  frame.humi = in[offset_humi];
  frame.mq135_adc = readU16(in, offset_mq135);
  frame.mq2_adc = readU16(in, offset_mq2);
  frame.rain_adc = readU16(in, offset_rain);
  frame.therm_adc = readU16(in, offset_therm_adc);
  frame.therm_c10 = static_cast<int16_t>(readU16(in, offset_therm_c10));
  frame.flame = in[offset_flame];
  frame.rain_wet = in[offset_rain_wet];
  frame.therm_hot = in[offset_therm_hot];
  frame.seq = in[offset_seq];
  frame.status = in[offset_status];
  return true;
}

}  // namespace app
