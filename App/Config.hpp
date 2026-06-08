#pragma once

#include <stdint.h>

namespace app {

enum class NodeRole : uint8_t
{
  Sensor = 1u,
  Monitor = 2u,
};

enum class AlarmState : uint8_t
{
  Normal,
  Warn,
  Danger,
  Waiting,
  Lost,
};

constexpr uint32_t usart_baudrate = 115200u;
constexpr uint32_t sensor_period_ms = 1000u;
constexpr uint32_t dht11_period_ms = 2100u;
constexpr uint32_t ui_period_ms = 500u;
constexpr uint32_t alarm_period_ms = 100u;
constexpr uint32_t node_timeout_ms = 5000u;
constexpr uint32_t mute_time_ms = 60000u;
constexpr uint32_t flash_log_period_ms = 10000u;

constexpr uint16_t rain_wet_adc_default = 1400u;
constexpr int16_t therm_warn_c10_default = 450;
constexpr int16_t therm_danger_c10_default = 700;

struct AlarmThresholds
{
  uint16_t air_warn;
  uint16_t smoke_warn;
  uint16_t smoke_danger;
  uint16_t rain_wet;
  int16_t therm_warn_c10;
  int16_t therm_danger_c10;
};

constexpr AlarmThresholds threshold_profiles[] =
{
  {2200u, 1800u, 2800u, rain_wet_adc_default, therm_warn_c10_default, therm_danger_c10_default},
  {1800u, 1400u, 2400u, 1200u, 400, 650},
  {2600u, 2200u, 3300u, 1800u, 500, 750},
};

constexpr uint8_t threshold_profile_count =
  static_cast<uint8_t>(sizeof(threshold_profiles) / sizeof(threshold_profiles[0]));

}  // namespace app
