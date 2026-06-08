#include "OledDisplay.hpp"

#include "App/BoardPins.hpp"
#include "App/Hal/Hardware.hpp"

#include "main.h"

#include <string.h>

namespace app {
namespace {

void font5x7(char c, uint8_t out[5])
{
  memset(out, 0, 5u);
  if ((c >= 'a') && (c <= 'z'))
  {
    c = static_cast<char>(c - 'a' + 'A');
  }

  switch (c)
  {
    case '0': { const uint8_t v[5] = {0x3Eu,0x51u,0x49u,0x45u,0x3Eu}; memcpy(out,v,5u); break; }
    case '1': { const uint8_t v[5] = {0x00u,0x42u,0x7Fu,0x40u,0x00u}; memcpy(out,v,5u); break; }
    case '2': { const uint8_t v[5] = {0x42u,0x61u,0x51u,0x49u,0x46u}; memcpy(out,v,5u); break; }
    case '3': { const uint8_t v[5] = {0x21u,0x41u,0x45u,0x4Bu,0x31u}; memcpy(out,v,5u); break; }
    case '4': { const uint8_t v[5] = {0x18u,0x14u,0x12u,0x7Fu,0x10u}; memcpy(out,v,5u); break; }
    case '5': { const uint8_t v[5] = {0x27u,0x45u,0x45u,0x45u,0x39u}; memcpy(out,v,5u); break; }
    case '6': { const uint8_t v[5] = {0x3Cu,0x4Au,0x49u,0x49u,0x30u}; memcpy(out,v,5u); break; }
    case '7': { const uint8_t v[5] = {0x01u,0x71u,0x09u,0x05u,0x03u}; memcpy(out,v,5u); break; }
    case '8': { const uint8_t v[5] = {0x36u,0x49u,0x49u,0x49u,0x36u}; memcpy(out,v,5u); break; }
    case '9': { const uint8_t v[5] = {0x06u,0x49u,0x49u,0x29u,0x1Eu}; memcpy(out,v,5u); break; }
    case 'A': { const uint8_t v[5] = {0x7Eu,0x11u,0x11u,0x11u,0x7Eu}; memcpy(out,v,5u); break; }
    case 'B': { const uint8_t v[5] = {0x7Fu,0x49u,0x49u,0x49u,0x36u}; memcpy(out,v,5u); break; }
    case 'C': { const uint8_t v[5] = {0x3Eu,0x41u,0x41u,0x41u,0x22u}; memcpy(out,v,5u); break; }
    case 'D': { const uint8_t v[5] = {0x7Fu,0x41u,0x41u,0x22u,0x1Cu}; memcpy(out,v,5u); break; }
    case 'E': { const uint8_t v[5] = {0x7Fu,0x49u,0x49u,0x49u,0x41u}; memcpy(out,v,5u); break; }
    case 'F': { const uint8_t v[5] = {0x7Fu,0x09u,0x09u,0x09u,0x01u}; memcpy(out,v,5u); break; }
    case 'G': { const uint8_t v[5] = {0x3Eu,0x41u,0x49u,0x49u,0x7Au}; memcpy(out,v,5u); break; }
    case 'H': { const uint8_t v[5] = {0x7Fu,0x08u,0x08u,0x08u,0x7Fu}; memcpy(out,v,5u); break; }
    case 'I': { const uint8_t v[5] = {0x00u,0x41u,0x7Fu,0x41u,0x00u}; memcpy(out,v,5u); break; }
    case 'J': { const uint8_t v[5] = {0x20u,0x40u,0x41u,0x3Fu,0x01u}; memcpy(out,v,5u); break; }
    case 'K': { const uint8_t v[5] = {0x7Fu,0x08u,0x14u,0x22u,0x41u}; memcpy(out,v,5u); break; }
    case 'L': { const uint8_t v[5] = {0x7Fu,0x40u,0x40u,0x40u,0x40u}; memcpy(out,v,5u); break; }
    case 'M': { const uint8_t v[5] = {0x7Fu,0x02u,0x0Cu,0x02u,0x7Fu}; memcpy(out,v,5u); break; }
    case 'N': { const uint8_t v[5] = {0x7Fu,0x04u,0x08u,0x10u,0x7Fu}; memcpy(out,v,5u); break; }
    case 'O': { const uint8_t v[5] = {0x3Eu,0x41u,0x41u,0x41u,0x3Eu}; memcpy(out,v,5u); break; }
    case 'P': { const uint8_t v[5] = {0x7Fu,0x09u,0x09u,0x09u,0x06u}; memcpy(out,v,5u); break; }
    case 'Q': { const uint8_t v[5] = {0x3Eu,0x41u,0x51u,0x21u,0x5Eu}; memcpy(out,v,5u); break; }
    case 'R': { const uint8_t v[5] = {0x7Fu,0x09u,0x19u,0x29u,0x46u}; memcpy(out,v,5u); break; }
    case 'S': { const uint8_t v[5] = {0x46u,0x49u,0x49u,0x49u,0x31u}; memcpy(out,v,5u); break; }
    case 'T': { const uint8_t v[5] = {0x01u,0x01u,0x7Fu,0x01u,0x01u}; memcpy(out,v,5u); break; }
    case 'U': { const uint8_t v[5] = {0x3Fu,0x40u,0x40u,0x40u,0x3Fu}; memcpy(out,v,5u); break; }
    case 'V': { const uint8_t v[5] = {0x1Fu,0x20u,0x40u,0x20u,0x1Fu}; memcpy(out,v,5u); break; }
    case 'W': { const uint8_t v[5] = {0x3Fu,0x40u,0x38u,0x40u,0x3Fu}; memcpy(out,v,5u); break; }
    case 'X': { const uint8_t v[5] = {0x63u,0x14u,0x08u,0x14u,0x63u}; memcpy(out,v,5u); break; }
    case 'Y': { const uint8_t v[5] = {0x07u,0x08u,0x70u,0x08u,0x07u}; memcpy(out,v,5u); break; }
    case 'Z': { const uint8_t v[5] = {0x61u,0x51u,0x49u,0x45u,0x43u}; memcpy(out,v,5u); break; }
    case ':': { const uint8_t v[5] = {0x00u,0x36u,0x36u,0x00u,0x00u}; memcpy(out,v,5u); break; }
    case '%': { const uint8_t v[5] = {0x23u,0x13u,0x08u,0x64u,0x62u}; memcpy(out,v,5u); break; }
    case '/': { const uint8_t v[5] = {0x20u,0x10u,0x08u,0x04u,0x02u}; memcpy(out,v,5u); break; }
    case '-': { const uint8_t v[5] = {0x08u,0x08u,0x08u,0x08u,0x08u}; memcpy(out,v,5u); break; }
    case '.': { const uint8_t v[5] = {0x00u,0x60u,0x60u,0x00u,0x00u}; memcpy(out,v,5u); break; }
    case ' ': default: break;
  }
}

}  // namespace

void OledDisplay::initBus()
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOB_CLK_ENABLE();

  gpio.Pin = pins::oled_scl_pin | pins::oled_sda_pin;
  gpio.Mode = GPIO_MODE_OUTPUT_OD;
  gpio.Pull = GPIO_PULLUP;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(pins::oled_port, &gpio);
  HAL_GPIO_WritePin(pins::oled_port, pins::oled_scl_pin | pins::oled_sda_pin, GPIO_PIN_SET);
}

void OledDisplay::delay()
{
  hal::delayUs(4u);
}

void OledDisplay::sda(bool high)
{
  HAL_GPIO_WritePin(pins::oled_port, pins::oled_sda_pin, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void OledDisplay::scl(bool high)
{
  HAL_GPIO_WritePin(pins::oled_port, pins::oled_scl_pin, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void OledDisplay::start()
{
  sda(true);
  scl(true);
  delay();
  sda(false);
  delay();
  scl(false);
}

void OledDisplay::stop()
{
  sda(false);
  scl(true);
  delay();
  sda(true);
  delay();
}

void OledDisplay::writeByte(uint8_t byte)
{
  for (uint8_t i = 0u; i < 8u; i++)
  {
    sda((byte & 0x80u) != 0u);
    delay();
    scl(true);
    delay();
    scl(false);
    byte <<= 1u;
  }

  sda(true);
  delay();
  scl(true);
  delay();
  scl(false);
}

void OledDisplay::write(uint8_t control, uint8_t data)
{
  start();
  writeByte(0x78u);
  writeByte(control);
  writeByte(data);
  stop();
}

void OledDisplay::cmd(uint8_t command)
{
  write(0x00u, command);
}

void OledDisplay::dataFill(uint8_t data, uint8_t count)
{
  start();
  writeByte(0x78u);
  writeByte(0x40u);
  for (uint8_t i = 0u; i < count; i++)
  {
    writeByte(data);
  }
  stop();
}

void OledDisplay::dataBuffer(const uint8_t *data, uint8_t len)
{
  start();
  writeByte(0x78u);
  writeByte(0x40u);
  for (uint8_t i = 0u; i < len; i++)
  {
    writeByte(data[i]);
  }
  stop();
}

void OledDisplay::initController()
{
  HAL_Delay(50u);
  cmd(0xAEu);
  cmd(0x20u);
  cmd(0x02u);
  cmd(0xB0u);
  cmd(0xC8u);
  cmd(0x00u);
  cmd(0x10u);
  cmd(0x40u);
  cmd(0x81u);
  cmd(0x7Fu);
  cmd(0xA1u);
  cmd(0xA6u);
  cmd(0xA8u);
  cmd(0x3Fu);
  cmd(0xA4u);
  cmd(0xD3u);
  cmd(0x00u);
  cmd(0xD5u);
  cmd(0x80u);
  cmd(0xD9u);
  cmd(0xF1u);
  cmd(0xDAu);
  cmd(0x12u);
  cmd(0xDBu);
  cmd(0x40u);
  cmd(0x8Du);
  cmd(0x14u);
  cmd(0xAFu);
}

void OledDisplay::clear()
{
  for (uint8_t page = 0u; page < 8u; page++)
  {
    setCursor(page, 0u);
    dataFill(0x00u, width_pixels);
  }
}

void OledDisplay::setCursor(uint8_t page, uint8_t col)
{
  cmd(static_cast<uint8_t>(0xB0u | (page & 0x07u)));
  cmd(static_cast<uint8_t>(0x00u | (col & 0x0Fu)));
  cmd(static_cast<uint8_t>(0x10u | ((col >> 4) & 0x0Fu)));
}

void OledDisplay::printLine(uint8_t page, const char *text)
{
  uint8_t pixels[width_pixels];
  uint8_t col = 0u;

  memset(pixels, 0, sizeof(pixels));
  while ((*text != '\0') && (static_cast<uint16_t>(col) + font_width <= width_pixels))
  {
    uint8_t font[5];
    font5x7(*text, font);
    text++;

    for (uint8_t i = 0u; i < 5u; i++)
    {
      pixels[col++] = font[i];
    }
    pixels[col++] = 0x00u;
  }

  setCursor(page, 0u);
  dataBuffer(pixels, width_pixels);
}

}  // namespace app
