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
constexpr uint32_t threshold_key_debounce_ms = 50u;

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

enum class ThresholdSensor : uint8_t
{
  Air = 0u,
  Smoke = 1u,
  Rain = 2u,
  Therm = 3u,
};

constexpr uint8_t threshold_sensor_count = 4u;
constexpr uint8_t threshold_level_count = 5u;
constexpr uint8_t default_threshold_level = 2u;
constexpr uint8_t custom_threshold_profile = 255u;

struct ThresholdLevels
{
  uint8_t air;
  uint8_t smoke;
  uint8_t rain;
  uint8_t therm;
};

constexpr uint16_t air_warn_levels[threshold_level_count] =
{
  1600u, 1800u, 2200u, 2400u, 2600u,
};

constexpr uint16_t smoke_warn_levels[threshold_level_count] =
{
  1200u, 1400u, 1800u, 2000u, 2200u,
};

constexpr uint16_t smoke_danger_levels[threshold_level_count] =
{
  2200u, 2400u, 2800u, 3050u, 3300u,
};

constexpr uint16_t rain_wet_levels[threshold_level_count] =
{
  1000u, 1200u, rain_wet_adc_default, 1600u, 1800u,
};

constexpr int16_t therm_warn_c10_levels[threshold_level_count] =
{
  380, 400, therm_warn_c10_default, 475, 500,
};

constexpr int16_t therm_danger_c10_levels[threshold_level_count] =
{
  620, 650, therm_danger_c10_default, 725, 750,
};

constexpr ThresholdLevels default_threshold_levels =
{
  default_threshold_level,
  default_threshold_level,
  default_threshold_level,
  default_threshold_level,
};

constexpr uint8_t normalizeThresholdLevel(uint8_t level)
{
  return level < threshold_level_count ? level : default_threshold_level;
}

constexpr AlarmThresholds thresholdsFromLevels(const ThresholdLevels &levels)
{
  return
  {
    air_warn_levels[normalizeThresholdLevel(levels.air)],
    smoke_warn_levels[normalizeThresholdLevel(levels.smoke)],
    smoke_danger_levels[normalizeThresholdLevel(levels.smoke)],
    rain_wet_levels[normalizeThresholdLevel(levels.rain)],
    therm_warn_c10_levels[normalizeThresholdLevel(levels.therm)],
    therm_danger_c10_levels[normalizeThresholdLevel(levels.therm)],
  };
}

constexpr bool allThresholdLevelsEqual(const ThresholdLevels &levels, uint8_t level)
{
  return (levels.air == level) &&
         (levels.smoke == level) &&
         (levels.rain == level) &&
         (levels.therm == level);
}

constexpr uint8_t compatibleThresholdProfile(const ThresholdLevels &levels)
{
  return allThresholdLevelsEqual(levels, default_threshold_level) ? 0u :
         allThresholdLevelsEqual(levels, 1u) ? 1u :
         allThresholdLevelsEqual(levels, 4u) ? 2u :
         custom_threshold_profile;
}

constexpr AlarmThresholds threshold_profiles[] =
{
  thresholdsFromLevels({2u, 2u, 2u, 2u}),
  thresholdsFromLevels({1u, 1u, 1u, 1u}),
  thresholdsFromLevels({4u, 4u, 4u, 4u}),
};

constexpr uint8_t threshold_profile_count =
  static_cast<uint8_t>(sizeof(threshold_profiles) / sizeof(threshold_profiles[0]));

}  // namespace app
