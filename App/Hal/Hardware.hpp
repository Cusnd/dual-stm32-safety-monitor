#pragma once

#include "main.h"

#include <stdint.h>

namespace app::hal {

constexpr uint8_t node_rx_buf_size = 128u;

class GpioPin
{
public:
  constexpr GpioPin(GPIO_TypeDef *port, uint16_t pin) : port_(port), pin_(pin) {}

  void write(bool high) const;
  void toggle() const;
  bool isSet() const;
  bool isReset() const;

private:
  GPIO_TypeDef *port_;
  uint16_t pin_;
};

class RxRingBuffer
{
public:
  void clear();
  bool pushFromIsr(uint8_t data);
  int read();
  uint16_t overflowCount() const;

private:
  volatile uint8_t buffer_[node_rx_buf_size];
  volatile uint8_t head_;
  volatile uint8_t tail_;
  volatile uint16_t overflow_count_;
};

void initDwtDelay();
void delayUs(uint32_t us);

void initDebugUsart1();
void initNodeUsart3(bool enable_rx_interrupt);
void sendUsartByte(USART_TypeDef *usart, uint8_t byte);
void sendUsartBuffer(USART_TypeDef *usart, const uint8_t *data, uint16_t len);
int readUsartByte(USART_TypeDef *usart);
uint16_t nodeUsartOverflowCount();
int writeDebugChar(int ch);
void handleUsart3Irq();

void initAdc1();
uint16_t readAdc1Channel(uint8_t channel);

}  // namespace app::hal
