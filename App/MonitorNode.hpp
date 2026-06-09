#pragma once

#include "App/Config.hpp"
#include "App/Drivers/BoardIo.hpp"
#include "App/Drivers/OledDisplay.hpp"
#include "App/Drivers/W25q64FlashLogger.hpp"
#include "App/Monitor/AlarmEvaluator.hpp"
#include "App/Protocol/FrameCodec.hpp"
#include "App/Protocol/FrameStreamDecoder.hpp"
#include "App/Protocol/SensorFrame.hpp"

#include <stdint.h>

namespace app {

class MonitorNode
{
public:
  void init();
  void run();

private:
  struct DebouncedButton
  {
    uint8_t raw;
    uint8_t stable;
    uint32_t changed_ms;
  };

  void processRx(uint32_t now);
  void updateButtons(uint32_t now);
  uint8_t pressedEdge(DebouncedButton &button, uint8_t raw_pressed, uint32_t now);
  uint8_t &selectedThresholdLevel();
  uint8_t selectedThresholdLevel() const;
  const char *selectedThresholdName() const;
  void updateAlarm(const AlarmEvaluation &alarm, uint32_t now);
  void updateDisplay(const AlarmEvaluation &alarm);
  void printFrontendJson(const SensorFrame &frame, const AlarmEvaluation &alarm) const;

  OledDisplay oled_;
  W25q64FlashLogger flash_;
  Buzzer buzzer_;
  Buttons buttons_;
  FrameStreamDecoder decoder_;
  SensorFrame latest_frame_;
  uint32_t last_rx_ms_;
  uint8_t have_rx_;
  uint8_t page_;
  ThresholdLevels threshold_levels_;
  uint8_t selected_threshold_sensor_;
  uint32_t mute_until_ms_;
  uint8_t k1_last_;
  uint8_t k2_last_;
  uint32_t k2_down_ms_;
  DebouncedButton threshold_select_button_;
  DebouncedButton threshold_level_button_;
  uint32_t last_ui_ms_;
  uint32_t last_alarm_ms_;
  uint32_t last_log_ms_;
  uint8_t last_logged_state_;
};

}  // namespace app
