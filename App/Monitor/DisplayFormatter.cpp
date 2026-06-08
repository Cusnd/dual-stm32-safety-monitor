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

}  // namespace

void formatMonitorDisplay(
  MonitorDisplayLines &lines,
  const SensorFrame &frame,
  const AlarmEvaluation &alarm,
  uint8_t page,
  uint8_t threshold_profile,
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

  snprintf(lines.text[0], MonitorDisplayLines::length, "PROFILE:%u", threshold_profile);
  snprintf(lines.text[1], MonitorDisplayLines::length, "AIR:%04u MQ2:%04u",
           thresholds.air_warn, thresholds.smoke_warn);
  snprintf(lines.text[2], MonitorDisplayLines::length, "RAIN:%04u HOT:%d",
           thresholds.rain_wet, thresholds.therm_danger_c10 / 10);
  snprintf(lines.text[3], MonitorDisplayLines::length, "SEQ:%03u F:%s L:%lu",
           frame.seq, flash_present ? "OK" : "NO",
           static_cast<unsigned long>(flash_records % 1000u));
}

}  // namespace app
