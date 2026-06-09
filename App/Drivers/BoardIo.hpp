#pragma once

#include <stdint.h>

namespace app {

class Buzzer
{
public:
  void init();
  void set(bool on);
};

class Buttons
{
public:
  void init();
  bool key1Pressed() const;
  bool key2Pressed() const;
  bool thresholdSelectPressed() const;
  bool thresholdLevelPressed() const;
};

}  // namespace app
