#include "AlarmEvaluator.hpp"

namespace app {
namespace {

bool isMuted(uint32_t now, uint32_t mute_until_ms)
{
  return static_cast<int32_t>(mute_until_ms - now) > 0;
}

bool hasThermAdc(const SensorFrame &frame)
{
  return (frame.status & sensor_status(SensorStatus::ThermAdcError)) == 0u;
}

bool hasDangerCondition(const SensorFrame &frame, const AlarmThresholds &thresholds)
{
  return (frame.flame != 0u) ||
         (frame.mq2_adc >= thresholds.smoke_danger) ||
         (frame.therm_hot != 0u) ||
         (hasThermAdc(frame) && (frame.therm_c10 >= thresholds.therm_danger_c10));
}

bool hasWarnCondition(const SensorFrame &frame, const AlarmThresholds &thresholds)
{
  return ((frame.status & sensor_status(SensorStatus::DhtError)) != 0u) ||
         (frame.mq135_adc >= thresholds.air_warn) ||
         (frame.mq2_adc >= thresholds.smoke_warn) ||
         (frame.rain_wet != 0u) ||
         (frame.rain_adc >= thresholds.rain_wet) ||
         (hasThermAdc(frame) && (frame.therm_c10 >= thresholds.therm_warn_c10));
}

}  // namespace

AlarmEvaluation evaluateAlarm(
  const SensorFrame &frame,
  bool have_rx,
  uint32_t now,
  uint32_t last_rx_ms,
  uint32_t mute_until_ms,
  const AlarmThresholds &thresholds)
{
  AlarmEvaluation evaluation = {};

  evaluation.lost = (static_cast<uint32_t>(now - last_rx_ms) > node_timeout_ms) ? 1u : 0u;
  evaluation.waiting = (!have_rx && (evaluation.lost == 0u)) ? 1u : 0u;
  evaluation.muted = isMuted(now, mute_until_ms) ? 1u : 0u;

  if ((evaluation.waiting != 0u) || (evaluation.lost != 0u))
  {
    evaluation.danger = 0u;
  }
  else
  {
    evaluation.danger = hasDangerCondition(frame, thresholds) ? 1u : 0u;
  }

  if (evaluation.waiting != 0u)
  {
    evaluation.warn = 0u;
  }
  else if (evaluation.lost != 0u)
  {
    evaluation.warn = 1u;
  }
  else
  {
    evaluation.warn = hasWarnCondition(frame, thresholds) ? 1u : 0u;
  }

  if (evaluation.danger != 0u)
  {
    evaluation.state = AlarmState::Danger;
  }
  else if (evaluation.waiting != 0u)
  {
    evaluation.state = AlarmState::Waiting;
  }
  else if (evaluation.lost != 0u)
  {
    evaluation.state = AlarmState::Lost;
  }
  else if (evaluation.warn != 0u)
  {
    evaluation.state = AlarmState::Warn;
  }
  else
  {
    evaluation.state = AlarmState::Normal;
  }

  return evaluation;
}

const char *alarmStateString(AlarmState state)
{
  switch (state)
  {
    case AlarmState::Danger:
      return "danger";
    case AlarmState::Waiting:
      return "waiting";
    case AlarmState::Lost:
      return "node_lost";
    case AlarmState::Warn:
      return "warn";
    case AlarmState::Normal:
    default:
      return "normal";
  }
}

}  // namespace app
