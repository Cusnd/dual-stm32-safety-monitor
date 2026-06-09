#pragma once

#include "AlarmEvaluator.hpp"
#include "App/Config.hpp"
#include "App/Protocol/SensorFrame.hpp"

#include <stdint.h>

namespace app {

struct MonitorDisplayLines
{
  static constexpr uint8_t count = 4u;
  static constexpr uint8_t length = 24u;

  char text[count][length];
};

void formatMonitorDisplay(
  MonitorDisplayLines &lines,
  const SensorFrame &frame,
  const AlarmEvaluation &alarm,
  uint8_t page,
  uint8_t selected_threshold_sensor,
  const ThresholdLevels &levels,
  const AlarmThresholds &thresholds,
  bool flash_present,
  uint32_t flash_records);

}  // namespace app
