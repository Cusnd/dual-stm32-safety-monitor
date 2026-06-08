#pragma once

#include "App/Config.hpp"
#include "App/Protocol/SensorFrame.hpp"

#include <stdint.h>

namespace app {

struct AlarmEvaluation
{
  AlarmState state;
  uint8_t waiting;
  uint8_t lost;
  uint8_t danger;
  uint8_t warn;
  uint8_t muted;
};

AlarmEvaluation evaluateAlarm(
  const SensorFrame &frame,
  bool have_rx,
  uint32_t now,
  uint32_t last_rx_ms,
  uint32_t mute_until_ms,
  const AlarmThresholds &thresholds);

const char *alarmStateString(AlarmState state);

}  // namespace app
