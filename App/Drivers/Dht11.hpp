#pragma once

#include <stdint.h>

namespace app {

class Dht11
{
public:
  void initGpio();
  bool read(uint8_t &temp, uint8_t &humi);

private:
  void setOutput();
  void setInput();
  bool waitLevel(int level, uint32_t timeout_us);
};

}  // namespace app
