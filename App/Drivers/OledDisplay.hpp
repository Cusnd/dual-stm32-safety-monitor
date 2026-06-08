#pragma once

#include <stdint.h>

namespace app {

class OledDisplay
{
public:
  static constexpr uint8_t width_pixels = 128u;
  static constexpr uint8_t font_width = 6u;

  void initBus();
  void initController();
  void clear();
  void printLine(uint8_t page, const char *text);

private:
  void delay();
  void sda(bool high);
  void scl(bool high);
  void start();
  void stop();
  void writeByte(uint8_t byte);
  void write(uint8_t control, uint8_t data);
  void cmd(uint8_t command);
  void dataFill(uint8_t data, uint8_t count);
  void dataBuffer(const uint8_t *data, uint8_t len);
  void setCursor(uint8_t page, uint8_t col);
};

}  // namespace app
