#include "FrameCodec.hpp"

namespace app {

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
  out[0] = head0;
  out[1] = head1;
  out[2] = payload_len;
  out[3] = version;
  out[4] = frame.temp;
  out[5] = frame.humi;
  out[6] = static_cast<uint8_t>(frame.mq135_adc >> 8);
  out[7] = static_cast<uint8_t>(frame.mq135_adc & 0xFFu);
  out[8] = static_cast<uint8_t>(frame.mq2_adc >> 8);
  out[9] = static_cast<uint8_t>(frame.mq2_adc & 0xFFu);
  out[10] = static_cast<uint8_t>(frame.rain_adc >> 8);
  out[11] = static_cast<uint8_t>(frame.rain_adc & 0xFFu);
  out[12] = static_cast<uint8_t>(frame.therm_adc >> 8);
  out[13] = static_cast<uint8_t>(frame.therm_adc & 0xFFu);
  out[14] = static_cast<uint8_t>(static_cast<uint16_t>(frame.therm_c10) >> 8);
  out[15] = static_cast<uint8_t>(static_cast<uint16_t>(frame.therm_c10) & 0xFFu);
  out[16] = frame.flame;
  out[17] = frame.rain_wet;
  out[18] = frame.therm_hot;
  out[19] = frame.seq;
  out[20] = frame.status;
  out[21] = checksum(&out[2], static_cast<uint8_t>(1u + payload_len));
  return total_len;
}

bool FrameCodec::decode(const uint8_t in[total_len], SensorFrame &frame)
{
  if ((in[0] != head0) || (in[1] != head1) ||
      (in[2] != payload_len) || (in[3] != version))
  {
    return false;
  }

  if (checksum(&in[2], static_cast<uint8_t>(1u + payload_len)) != in[21])
  {
    return false;
  }

  frame.temp = in[4];
  frame.humi = in[5];
  frame.mq135_adc = static_cast<uint16_t>((static_cast<uint16_t>(in[6]) << 8) | in[7]);
  frame.mq2_adc = static_cast<uint16_t>((static_cast<uint16_t>(in[8]) << 8) | in[9]);
  frame.rain_adc = static_cast<uint16_t>((static_cast<uint16_t>(in[10]) << 8) | in[11]);
  frame.therm_adc = static_cast<uint16_t>((static_cast<uint16_t>(in[12]) << 8) | in[13]);
  frame.therm_c10 = static_cast<int16_t>((static_cast<uint16_t>(in[14]) << 8) | in[15]);
  frame.flame = in[16];
  frame.rain_wet = in[17];
  frame.therm_hot = in[18];
  frame.seq = in[19];
  frame.status = in[20];
  return true;
}

}  // namespace app
