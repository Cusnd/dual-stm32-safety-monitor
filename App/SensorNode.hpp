#pragma once

#include "App/Drivers/Dht11.hpp"
#include "App/Protocol/SensorFrame.hpp"

#include <stdint.h>

namespace app {

class SensorNode
{
public:
  void init();
  void run();

private:
  void initGpio();
  uint16_t filter(uint16_t previous, uint16_t sample, bool valid);
  int16_t thermistorAdcToC10(uint16_t adc, bool &valid);
  void sendFrame(const SensorFrame &frame);

  Dht11 dht_;
  uint32_t last_sensor_ms_;
  uint32_t last_dht_ms_;
  uint8_t seq_;
  uint8_t temp_;
  uint8_t humi_;
  uint8_t dht_ok_;
  uint8_t avg_valid_;
  uint16_t mq135_avg_;
  uint16_t mq2_avg_;
  uint16_t rain_avg_;
  uint16_t therm_avg_;
};

}  // namespace app
