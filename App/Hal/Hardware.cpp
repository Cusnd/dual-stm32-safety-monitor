#include "Hardware.hpp"

#include "App/Config.hpp"

namespace app::hal {
namespace {

RxRingBuffer g_node_rx;

}  // namespace

void GpioPin::write(bool high) const
{
  HAL_GPIO_WritePin(port_, pin_, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void GpioPin::toggle() const
{
  HAL_GPIO_TogglePin(port_, pin_);
}

bool GpioPin::isSet() const
{
  return HAL_GPIO_ReadPin(port_, pin_) == GPIO_PIN_SET;
}

bool GpioPin::isReset() const
{
  return HAL_GPIO_ReadPin(port_, pin_) == GPIO_PIN_RESET;
}

void RxRingBuffer::clear()
{
  head_ = 0u;
  tail_ = 0u;
}

bool RxRingBuffer::pushFromIsr(uint8_t data)
{
  const uint8_t next = static_cast<uint8_t>((head_ + 1u) % node_rx_buf_size);
  if (next == tail_)
  {
    return false;
  }

  buffer_[head_] = data;
  head_ = next;
  return true;
}

int RxRingBuffer::read()
{
  if (head_ == tail_)
  {
    return -1;
  }

  const uint8_t data = buffer_[tail_];
  tail_ = static_cast<uint8_t>((tail_ + 1u) % node_rx_buf_size);
  return static_cast<int>(data);
}

void initDwtDelay()
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0u;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void delayUs(uint32_t us)
{
  const uint32_t ticks = us * (SystemCoreClock / 1000000u);
  const uint32_t start = DWT->CYCCNT;
  while (static_cast<uint32_t>(DWT->CYCCNT - start) < ticks)
  {
  }
}

void initDebugUsart1()
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_USART1_CLK_ENABLE();

  gpio.Pin = GPIO_PIN_9;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &gpio);

  gpio.Pin = GPIO_PIN_10;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &gpio);

  USART1->BRR = static_cast<uint16_t>((72000000u + (usart_baudrate / 2u)) / usart_baudrate);
  USART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
}

void initNodeUsart3(bool enable_rx_interrupt)
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_USART3_CLK_ENABLE();

  gpio.Pin = GPIO_PIN_10;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &gpio);

  gpio.Pin = GPIO_PIN_11;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &gpio);

  USART3->BRR = static_cast<uint16_t>((36000000u + (usart_baudrate / 2u)) / usart_baudrate);
  USART3->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
  g_node_rx.clear();

  if (enable_rx_interrupt)
  {
    USART3->CR1 |= USART_CR1_RXNEIE;
    HAL_NVIC_SetPriority(USART3_IRQn, 1u, 0u);
    HAL_NVIC_EnableIRQ(USART3_IRQn);
  }
}

void sendUsartByte(USART_TypeDef *usart, uint8_t byte)
{
  while ((usart->SR & USART_SR_TXE) == 0u)
  {
  }
  usart->DR = byte;
}

void sendUsartBuffer(USART_TypeDef *usart, const uint8_t *data, uint16_t len)
{
  for (uint16_t i = 0u; i < len; i++)
  {
    sendUsartByte(usart, data[i]);
  }
}

int readUsartByte(USART_TypeDef *usart)
{
  if (usart == USART3)
  {
    return g_node_rx.read();
  }

  if ((usart->SR & USART_SR_RXNE) == 0u)
  {
    return -1;
  }
  return static_cast<int>(usart->DR & 0xFFu);
}

int writeDebugChar(int ch)
{
  if (ch == '\n')
  {
    sendUsartByte(USART1, static_cast<uint8_t>('\r'));
  }
  sendUsartByte(USART1, static_cast<uint8_t>(ch));
  return ch;
}

void handleUsart3Irq()
{
  if ((USART3->SR & (USART_SR_RXNE | USART_SR_ORE)) != 0u)
  {
    const uint8_t data = static_cast<uint8_t>(USART3->DR & 0xFFu);
    (void)g_node_rx.pushFromIsr(data);
  }
}

void initAdc1()
{
  __HAL_RCC_ADC1_CLK_ENABLE();

  RCC->CFGR &= ~RCC_CFGR_ADCPRE;
  RCC->CFGR |= RCC_CFGR_ADCPRE_DIV6;

  ADC1->CR1 = 0u;
  ADC1->CR2 = ADC_CR2_ADON | ADC_CR2_EXTTRIG | ADC_CR2_EXTSEL;
  ADC1->SMPR2 |= ADC_SMPR2_SMP4 | ADC_SMPR2_SMP5 | ADC_SMPR2_SMP6 | ADC_SMPR2_SMP7;
  delayUs(10u);

  ADC1->CR2 |= ADC_CR2_RSTCAL;
  while ((ADC1->CR2 & ADC_CR2_RSTCAL) != 0u)
  {
  }
  ADC1->CR2 |= ADC_CR2_CAL;
  while ((ADC1->CR2 & ADC_CR2_CAL) != 0u)
  {
  }
}

uint16_t readAdc1Channel(uint8_t channel)
{
  ADC1->SQR1 = 0u;
  ADC1->SQR3 = channel & ADC_SQR3_SQ1;
  ADC1->SR = 0u;
  ADC1->CR2 |= ADC_CR2_SWSTART;
  while ((ADC1->SR & ADC_SR_EOC) == 0u)
  {
  }
  return static_cast<uint16_t>(ADC1->DR & 0x0FFFu);
}

}  // namespace app::hal
