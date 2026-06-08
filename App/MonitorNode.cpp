#include "MonitorNode.hpp"

#include "App/Hal/Hardware.hpp"
#include "App/Monitor/DisplayFormatter.hpp"

#include "main.h"

#include <stdio.h>

namespace app {

void MonitorNode::init()
{
  decoder_.reset();
  latest_frame_ = SensorFrame{};
  last_rx_ms_ = 0u;
  have_rx_ = 0u;
  page_ = 0u;
  threshold_profile_ = 0u;
  mute_until_ms_ = 0u;
  k1_last_ = 0u;
  k2_last_ = 0u;
  k2_down_ms_ = 0u;
  last_ui_ms_ = 0u;
  last_alarm_ms_ = 0u;
  last_log_ms_ = 0u;
  last_logged_state_ = 0xFFu;

  buzzer_.init();
  oled_.initBus();

  flash_.init();
  oled_.initController();
  oled_.clear();
  oled_.printLine(0u, "MONITOR NODE");
  oled_.printLine(2u, "WAIT SENSOR");
  printf("\r\n[MONITOR] boot, USART1 debug ready, USART3 link ready, flash=%s\r\n",
         flash_.present() ? "ok" : "none");
}

void MonitorNode::run()
{
  have_rx_ = 0u;
  last_rx_ms_ = HAL_GetTick();

  while (1)
  {
    const uint32_t now = HAL_GetTick();
    const AlarmThresholds &thresholds = threshold_profiles[threshold_profile_];

    processRx(now);
    updateButtons(now);
    flash_.process();

    const AlarmEvaluation alarm = evaluateAlarm(
      latest_frame_, have_rx_ != 0u, now, last_rx_ms_, mute_until_ms_, thresholds);

    if (static_cast<uint32_t>(now - last_alarm_ms_) >= alarm_period_ms)
    {
      updateAlarm(alarm, now);
      last_alarm_ms_ = now;
    }

    if (static_cast<uint32_t>(now - last_ui_ms_) >= ui_period_ms)
    {
      updateDisplay(alarm);
      last_ui_ms_ = now;
    }

    if (flash_.present() && (have_rx_ != 0u) && (alarm.lost == 0u))
    {
      const uint8_t state_value = static_cast<uint8_t>(alarm.state);
      if ((state_value != last_logged_state_) ||
          (static_cast<uint32_t>(now - last_log_ms_) >= flash_log_period_ms))
      {
        if (flash_.logFrame(latest_frame_, alarm.state, threshold_profile_, alarm.muted != 0u))
        {
          last_log_ms_ = now;
          last_logged_state_ = state_value;
        }
      }
    }
  }
}

void MonitorNode::processRx(uint32_t now)
{
  int rx;

  while ((rx = hal::readUsartByte(USART3)) >= 0)
  {
    const uint8_t b = static_cast<uint8_t>(rx);
    SensorFrame frame;

    switch (decoder_.push(b, frame))
    {
      case FrameStreamDecoder::Result::FrameReady: {
        latest_frame_ = frame;
        have_rx_ = 1u;
        last_rx_ms_ = now;
        const AlarmEvaluation alarm = evaluateAlarm(
          latest_frame_, true, now, last_rx_ms_, mute_until_ms_,
          threshold_profiles[threshold_profile_]);
        printf("[MONITOR] rx v%u seq=%u t=%u h=%u mq135=%u mq2=%u rain=%u therm=%d.%dC flame=%u status=0x%02X\r\n",
               FrameCodec::version,
               frame.seq, frame.temp, frame.humi, frame.mq135_adc, frame.mq2_adc,
               frame.rain_adc, frame.therm_c10 / 10,
               frame.therm_c10 < 0 ? -(frame.therm_c10 % 10) : (frame.therm_c10 % 10),
               frame.flame, frame.status);
        printFrontendJson(frame, alarm);
        break;
      }

      case FrameStreamDecoder::Result::BadFrame:
        printf("[MONITOR] bad frame\r\n");
        break;

      case FrameStreamDecoder::Result::NeedMore:
      default:
        break;
    }
  }
}

void MonitorNode::updateButtons(uint32_t now)
{
  const uint8_t k1 = buttons_.key1Pressed() ? 1u : 0u;
  const uint8_t k2 = buttons_.key2Pressed() ? 1u : 0u;

  if ((k1 != 0u) && (k1_last_ == 0u))
  {
    page_ ^= 1u;
  }

  if ((k2 != 0u) && (k2_last_ == 0u))
  {
    k2_down_ms_ = now;
  }
  else if ((k2 == 0u) && (k2_last_ != 0u))
  {
    const uint32_t held = now - k2_down_ms_;
    if (held >= 1200u)
    {
      threshold_profile_ = static_cast<uint8_t>((threshold_profile_ + 1u) % threshold_profile_count);
      printf("[MONITOR] threshold profile=%u\r\n", threshold_profile_);
    }
    else
    {
      mute_until_ms_ = now + mute_time_ms;
      printf("[MONITOR] buzzer muted for 60s\r\n");
    }
  }

  k1_last_ = k1;
  k2_last_ = k2;
}

void MonitorNode::printFrontendJson(const SensorFrame &frame, const AlarmEvaluation &alarm) const
{
  printf("{\"type\":\"sensor\",\"schemaVersion\":%u,\"seq\":%u,\"tickMs\":%lu,\"tempC\":%u,"
         "\"humidityPct\":%u,\"mq135Raw\":%u,\"mq2Raw\":%u,\"rainRaw\":%u,"
         "\"thermRaw\":%u,\"thermC10\":%d,\"rainWet\":%u,\"thermHot\":%u,\"flame\":%u,"
         "\"status\":%u,\"alarm\":\"%s\",\"thresholdProfile\":%u,"
         "\"mute\":%u,\"flashReady\":%u,\"flashRecords\":%lu,\"externalRgb\":%u}\n",
         static_cast<unsigned int>(FrameCodec::version),
         static_cast<unsigned int>(frame.seq),
         static_cast<unsigned long>(HAL_GetTick()),
         static_cast<unsigned int>(frame.temp),
         static_cast<unsigned int>(frame.humi),
         static_cast<unsigned int>(frame.mq135_adc),
         static_cast<unsigned int>(frame.mq2_adc),
         static_cast<unsigned int>(frame.rain_adc),
         static_cast<unsigned int>(frame.therm_adc),
         static_cast<int>(frame.therm_c10),
         static_cast<unsigned int>(frame.rain_wet),
         static_cast<unsigned int>(frame.therm_hot),
         static_cast<unsigned int>(frame.flame),
         static_cast<unsigned int>(frame.status),
         alarmStateString(alarm.state),
         static_cast<unsigned int>(threshold_profile_),
         static_cast<unsigned int>(alarm.muted),
         flash_.present() ? 1u : 0u,
         static_cast<unsigned long>(flash_.recordCount()),
         0u);
}

void MonitorNode::updateAlarm(const AlarmEvaluation &alarm, uint32_t now)
{
  if (alarm.state == AlarmState::Danger)
  {
    buzzer_.set((alarm.muted == 0u) && ((now / 150u) % 2u == 0u));
  }
  else if (alarm.state == AlarmState::Waiting)
  {
    buzzer_.set(false);
  }
  else if (alarm.state == AlarmState::Lost)
  {
    buzzer_.set((alarm.muted == 0u) && ((now / 700u) % 2u == 0u));
  }
  else if (alarm.state == AlarmState::Warn)
  {
    buzzer_.set(false);
  }
  else
  {
    buzzer_.set(false);
  }
}

void MonitorNode::updateDisplay(const AlarmEvaluation &alarm)
{
  MonitorDisplayLines lines;
  formatMonitorDisplay(lines, latest_frame_, alarm, page_, threshold_profile_,
                       threshold_profiles[threshold_profile_], flash_.present(),
                       flash_.recordCount());

  for (uint8_t i = 0u; i < MonitorDisplayLines::count; i++)
  {
    oled_.printLine(i, lines.text[i]);
  }
}

}  // namespace app
