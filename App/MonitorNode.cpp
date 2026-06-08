#include "MonitorNode.hpp"

#include "App/Hal/Hardware.hpp"

#include "main.h"

#include <stdio.h>

namespace app {

void MonitorNode::init()
{
  rx_pos_ = 0u;
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
  led_.set(0u, 0u, 1u);
  external_rgb_.init();
  external_rgb_.setColor(0u, 0u, 24u);

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

    processRx();
    updateButtons();

    if (static_cast<uint32_t>(now - last_alarm_ms_) >= alarm_period_ms)
    {
      updateAlarm();
      last_alarm_ms_ = now;
    }

    if (static_cast<uint32_t>(now - last_ui_ms_) >= ui_period_ms)
    {
      updateDisplay();
      last_ui_ms_ = now;
    }

    if (flash_.present() && (have_rx_ != 0u) && !nodeLost())
    {
      const AlarmState state = danger() ? AlarmState::Danger : (warn() ? AlarmState::Warn : AlarmState::Normal);
      const uint8_t state_value = static_cast<uint8_t>(state);
      if ((state_value != last_logged_state_) ||
          (static_cast<uint32_t>(now - last_log_ms_) >= flash_log_period_ms))
      {
        flash_.logFrame(latest_frame_, state, threshold_profile_, muted());
        last_log_ms_ = now;
        last_logged_state_ = state_value;
      }
    }
  }
}

void MonitorNode::processRx()
{
  int rx;

  while ((rx = hal::readUsartByte(USART3)) >= 0)
  {
    const uint8_t b = static_cast<uint8_t>(rx);

    if (rx_pos_ == 0u)
    {
      if (b != FrameCodec::head0)
      {
        continue;
      }
    }
    else if ((rx_pos_ == 1u) && (b != FrameCodec::head1))
    {
      rx_pos_ = (b == FrameCodec::head0) ? 1u : 0u;
      if (rx_pos_ == 1u)
      {
        rx_buf_[0] = FrameCodec::head0;
      }
      continue;
    }

    rx_buf_[rx_pos_++] = b;
    if (rx_pos_ >= FrameCodec::total_len)
    {
      SensorFrame frame;
      if (FrameCodec::decode(rx_buf_, frame))
      {
        latest_frame_ = frame;
        have_rx_ = 1u;
        last_rx_ms_ = HAL_GetTick();
        printf("[MONITOR] rx v%u seq=%u t=%u h=%u mq135=%u mq2=%u rain=%u therm=%d.%dC flame=%u status=0x%02X\r\n",
               FrameCodec::version,
               frame.seq, frame.temp, frame.humi, frame.mq135_adc, frame.mq2_adc,
               frame.rain_adc, frame.therm_c10 / 10,
               frame.therm_c10 < 0 ? -(frame.therm_c10 % 10) : (frame.therm_c10 % 10),
               frame.flame, frame.status);
        printFrontendJson(frame);
      }
      else
      {
        printf("[MONITOR] bad frame\r\n");
      }
      rx_pos_ = 0u;
    }
  }
}

void MonitorNode::updateButtons()
{
  const uint32_t now = HAL_GetTick();
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

bool MonitorNode::linkWaiting() const
{
  return (have_rx_ == 0u) && !nodeLost();
}

bool MonitorNode::nodeLost() const
{
  return static_cast<uint32_t>(HAL_GetTick() - last_rx_ms_) > node_timeout_ms;
}

bool MonitorNode::danger() const
{
  const AlarmThresholds &th = threshold_profiles[threshold_profile_];
  if (linkWaiting() || nodeLost())
  {
    return false;
  }
  return (latest_frame_.flame != 0u) ||
         (latest_frame_.mq2_adc >= th.smoke_danger) ||
         (latest_frame_.therm_hot != 0u) ||
         (((latest_frame_.status & sensor_status(SensorStatus::ThermAdcError)) == 0u) &&
          (latest_frame_.therm_c10 >= th.therm_danger_c10));
}

bool MonitorNode::warn() const
{
  const AlarmThresholds &th = threshold_profiles[threshold_profile_];
  if (linkWaiting())
  {
    return false;
  }
  if (nodeLost())
  {
    return true;
  }
  return ((latest_frame_.status & sensor_status(SensorStatus::DhtError)) != 0u) ||
         (latest_frame_.mq135_adc >= th.air_warn) ||
         (latest_frame_.mq2_adc >= th.smoke_warn) ||
         (latest_frame_.rain_wet != 0u) ||
         (latest_frame_.rain_adc >= th.rain_wet) ||
         (((latest_frame_.status & sensor_status(SensorStatus::ThermAdcError)) == 0u) &&
          (latest_frame_.therm_c10 >= th.therm_warn_c10));
}

bool MonitorNode::muted() const
{
  const uint32_t now = HAL_GetTick();
  return static_cast<int32_t>(mute_until_ms_ - now) > 0;
}

const char *MonitorNode::alarmStateString() const
{
  if (danger())
  {
    return "danger";
  }
  if (linkWaiting())
  {
    return "waiting";
  }
  if (nodeLost())
  {
    return "node_lost";
  }
  if (warn())
  {
    return "warn";
  }
  return "normal";
}

void MonitorNode::printFrontendJson(const SensorFrame &frame) const
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
         alarmStateString(),
         static_cast<unsigned int>(threshold_profile_),
         muted() ? 1u : 0u,
         flash_.present() ? 1u : 0u,
         static_cast<unsigned long>(flash_.recordCount()),
         external_rgb_.ready() ? 1u : 0u);
}

void MonitorNode::updateAlarm()
{
  const uint32_t now = HAL_GetTick();
  const bool is_muted = muted();

  if (danger())
  {
    led_.set(1u, 0u, 0u);
    external_rgb_.setColor(((now / 150u) % 2u == 0u) ? 96u : 18u, 0u, 0u);
    buzzer_.set(!is_muted && ((now / 150u) % 2u == 0u));
  }
  else if (linkWaiting())
  {
    led_.set(0u, 0u, 1u);
    external_rgb_.setColor(0u, 0u, 28u);
    buzzer_.set(false);
  }
  else if (nodeLost())
  {
    led_.set(0u, 0u, 1u);
    external_rgb_.setColor(0u, 0u, ((now / 700u) % 2u == 0u) ? 80u : 12u);
    buzzer_.set(!is_muted && ((now / 700u) % 2u == 0u));
  }
  else if (warn())
  {
    led_.set(1u, 1u, 0u);
    external_rgb_.setColor(72u, 32u, 0u);
    buzzer_.set(false);
  }
  else
  {
    led_.set(0u, 1u, 0u);
    external_rgb_.setColor(0u, 54u, 0u);
    buzzer_.set(false);
  }
}

void MonitorNode::updateDisplay()
{
  char line[24];
  const bool waiting = linkWaiting();
  const bool lost = nodeLost();

  if (page_ == 0u)
  {
    oled_.printLine(0u, waiting ? "WAIT SENSOR" :
                        (lost ? "NODE LOST" :
                         (danger() ? "STATE DANGER" :
                          (warn() ? "STATE WARN" : "STATE NORMAL"))));
    snprintf(line, sizeof(line), "T:%02uC H:%02u%%", latest_frame_.temp, latest_frame_.humi);
    oled_.printLine(1u, line);
    snprintf(line, sizeof(line), "AIR:%04u MQ2:%04u", latest_frame_.mq135_adc,
             latest_frame_.mq2_adc);
    oled_.printLine(2u, line);
    snprintf(line, sizeof(line), "R:%04u N:%d.%d", latest_frame_.rain_adc,
             latest_frame_.therm_c10 / 10,
             latest_frame_.therm_c10 < 0 ? -(latest_frame_.therm_c10 % 10) :
             (latest_frame_.therm_c10 % 10));
    oled_.printLine(3u, line);
  }
  else
  {
    const AlarmThresholds &th = threshold_profiles[threshold_profile_];
    snprintf(line, sizeof(line), "PROFILE:%u", threshold_profile_);
    oled_.printLine(0u, line);
    snprintf(line, sizeof(line), "AIR:%04u MQ2:%04u", th.air_warn, th.smoke_warn);
    oled_.printLine(1u, line);
    snprintf(line, sizeof(line), "RAIN:%04u HOT:%d", th.rain_wet, th.therm_danger_c10 / 10);
    oled_.printLine(2u, line);
    snprintf(line, sizeof(line), "SEQ:%03u F:%s L:%lu", latest_frame_.seq,
             flash_.present() ? "OK" : "NO",
             static_cast<unsigned long>(flash_.recordCount() % 1000u));
    oled_.printLine(3u, line);
  }
}

}  // namespace app
