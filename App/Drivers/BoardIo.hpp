#pragma once

#include <stdint.h>

namespace app {

class BoardRgb
{
public:
  void set(uint8_t red, uint8_t green, uint8_t blue);
};

class Buzzer
{
public:
  void init();
  void set(bool on);
};

class Buttons
{
public:
  bool key1Pressed() const;
  bool key2Pressed() const;
};

}  // namespace app
