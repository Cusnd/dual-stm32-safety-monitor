#include "DisplayFormatter.hpp"

#include <stdio.h>
#include <string.h>

namespace app {
namespace {

const char *stateTitle(const AlarmEvaluation &alarm)
{
  switch (alarm.state)
  {
    case AlarmState::Waiting:
      return "WAIT SENSOR";
    case AlarmState::Lost:
      return "NODE LOST";
    case AlarmState::Danger:
      return "STATE DANGER";
    case AlarmState::Warn:
      return "STATE WARN";
    case AlarmState::Normal:
    default:
      return "STATE NORMAL";
  }
}

int tempFraction(int16_t c10)
{
  return (c10 < 0) ? -(c10 % 10) : (c10 % 10);
}

const char *thresholdSensorName(uint8_t sensor)
{
  switch (static_cast<ThresholdSensor>(sensor))
  {
    case ThresholdSensor::Smoke:
      return "MQ2";
    case ThresholdSensor::Rain:
      return "RAIN";
    case ThresholdSensor::Therm:
      return "THERM";
    case ThresholdSensor::Air:
    default:
      return "MQ135";
  }
}

uint8_t thresholdLevelForSensor(const ThresholdLevels &levels, uint8_t sensor)
{
  switch (static_cast<ThresholdSensor>(sensor))
  {
    case ThresholdSensor::Smoke:
      return normalizeThresholdLevel(levels.smoke);
    case ThresholdSensor::Rain:
      return normalizeThresholdLevel(levels.rain);
    case ThresholdSensor::Therm:
      return normalizeThresholdLevel(levels.therm);
    case ThresholdSensor::Air:
    default:
      return normalizeThresholdLevel(levels.air);
  }
}

void formatSelectedThresholdLine(
  char *out,
  uint8_t len,
  uint8_t sensor,
  const AlarmThresholds &thresholds)
{
  switch (static_cast<ThresholdSensor>(sensor))
  {
    case ThresholdSensor::Smoke:
      snprintf(out, len, "WARN:%04u D:%04u", thresholds.smoke_warn, thresholds.smoke_danger);
      break;

    case ThresholdSensor::Rain:
      snprintf(out, len, "WET ADC:%04u", thresholds.rain_wet);
      break;

    case ThresholdSensor::Therm:
      snprintf(out, len, "W:%d.%d D:%d.%dC",
               thresholds.therm_warn_c10 / 10, tempFraction(thresholds.therm_warn_c10),
               thresholds.therm_danger_c10 / 10, tempFraction(thresholds.therm_danger_c10));
      break;

    case ThresholdSensor::Air:
    default:
      snprintf(out, len, "WARN ADC:%04u", thresholds.air_warn);
      break;
  }
}

}  // namespace

void formatMonitorDisplay(
  MonitorDisplayLines &lines,
  const SensorFrame &frame,
  const AlarmEvaluation &alarm,
  uint8_t page,
  uint8_t selected_threshold_sensor,
  const ThresholdLevels &levels,
  const AlarmThresholds &thresholds,
  bool flash_present,
  uint32_t flash_records)
{
  memset(&lines, 0, sizeof(lines));

  if (page == 0u)
  {
    snprintf(lines.text[0], MonitorDisplayLines::length, "%s", stateTitle(alarm));
    snprintf(lines.text[1], MonitorDisplayLines::length, "T:%02uC H:%02u%%",
             frame.temp, frame.humi);
    snprintf(lines.text[2], MonitorDisplayLines::length, "AIR:%04u MQ2:%04u",
             frame.mq135_adc, frame.mq2_adc);
    snprintf(lines.text[3], MonitorDisplayLines::length, "R:%04u N:%d.%d",
             frame.rain_adc, frame.therm_c10 / 10, tempFraction(frame.therm_c10));
    return;
  }

  selected_threshold_sensor =
    selected_threshold_sensor < threshold_sensor_count ? selected_threshold_sensor : 0u;
  snprintf(lines.text[0], MonitorDisplayLines::length, "SEL:%s L%u/%u",
           thresholdSensorName(selected_threshold_sensor),
           static_cast<unsigned int>(thresholdLevelForSensor(levels, selected_threshold_sensor) + 1u),
           static_cast<unsigned int>(threshold_level_count));
  formatSelectedThresholdLine(
    lines.text[1], MonitorDisplayLines::length, selected_threshold_sensor, thresholds);
  snprintf(lines.text[2], MonitorDisplayLines::length, "A%u M%u R%u T%u",
           static_cast<unsigned int>(normalizeThresholdLevel(levels.air) + 1u),
           static_cast<unsigned int>(normalizeThresholdLevel(levels.smoke) + 1u),
           static_cast<unsigned int>(normalizeThresholdLevel(levels.rain) + 1u),
           static_cast<unsigned int>(normalizeThresholdLevel(levels.therm) + 1u));
  snprintf(lines.text[3], MonitorDisplayLines::length, "SEQ:%03u F:%s L:%lu",
           frame.seq, flash_present ? "OK" : "NO",
           static_cast<unsigned long>(flash_records % 1000u));
}

}  // namespace app
