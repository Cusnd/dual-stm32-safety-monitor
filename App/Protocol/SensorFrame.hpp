#pragma once

#include <stdint.h>

namespace app {

struct SensorFrame
{
  uint8_t temp;
  uint8_t humi;
  uint16_t mq135_adc;
  uint16_t mq2_adc;
  uint16_t rain_adc;
  uint16_t therm_adc;
  int16_t therm_c10;
  uint8_t flame;
  uint8_t rain_wet;
  uint8_t therm_hot;
  uint8_t seq;
  uint8_t status;
};

enum class SensorStatus : uint8_t
{
  None = 0x00u,
  DhtError = 0x01u,
  ThermHotDigital = 0x02u,
  RainWet = 0x04u,
  ThermAdcError = 0x08u,
};

constexpr uint8_t sensor_status(SensorStatus status)
{
  return static_cast<uint8_t>(status);
}

}  // namespace app
