#include "Ws2813Led.hpp"

#include "App/BoardPins.hpp"

#include "main.h"

namespace app {

void Ws2813Led::init()
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_TIM3_CLK_ENABLE();
  __HAL_RCC_DMA1_CLK_ENABLE();

  gpio.Pin = pins::external_rgb_pin;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(pins::external_rgb_port, &gpio);

  TIM3->PSC = 0u;
  TIM3->ARR = timer_period;
  TIM3->CCR1 = 0u;
  TIM3->CCMR1 &= ~(TIM_CCMR1_OC1M | TIM_CCMR1_CC1S);
  TIM3->CCMR1 |= TIM_CCMR1_OC1PE | TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2;
  TIM3->CCER |= TIM_CCER_CC1E;
  TIM3->DIER |= TIM_DIER_CC1DE;
  TIM3->CR1 |= TIM_CR1_ARPE;
  TIM3->EGR = TIM_EGR_UG;

  DMA1_Channel6->CCR = 0u;
  DMA1_Channel6->CPAR = reinterpret_cast<uint32_t>(&TIM3->CCR1);
  DMA1->IFCR = DMA_IFCR_CGIF6 | DMA_IFCR_CTCIF6 | DMA_IFCR_CHTIF6 | DMA_IFCR_CTEIF6;

  ready_ = 1u;
  last_r_ = 0xFFu;
  last_g_ = 0xFFu;
  last_b_ = 0xFFu;
}

void Ws2813Led::setColor(uint8_t red, uint8_t green, uint8_t blue)
{
  if (ready_ == 0u)
  {
    return;
  }
  if ((red == last_r_) && (green == last_g_) && (blue == last_b_))
  {
    return;
  }

  last_r_ = red;
  last_g_ = green;
  last_b_ = blue;
  fillBuffer(red, green, blue);
  sendBuffer(buffer_len);
}

bool Ws2813Led::ready() const
{
  return ready_ != 0u;
}

void Ws2813Led::fillBuffer(uint8_t red, uint8_t green, uint8_t blue)
{
  const uint8_t grb[3] = {green, red, blue};
  uint16_t pos = 0u;

  for (uint8_t led = 0u; led < led_count; led++)
  {
    for (uint8_t color = 0u; color < 3u; color++)
    {
      uint8_t value = grb[color];
      for (uint8_t bit = 0u; bit < 8u; bit++)
      {
        dma_buffer_[pos++] = (value & 0x80u) ? code1_ccr : code0_ccr;
        value <<= 1u;
      }
    }
  }

  while (pos < buffer_len)
  {
    dma_buffer_[pos++] = 0u;
  }
}

void Ws2813Led::sendBuffer(uint16_t len)
{
  const uint32_t start = HAL_GetTick();

  DMA1_Channel6->CCR &= ~DMA_CCR_EN;
  DMA1_Channel6->CMAR = reinterpret_cast<uint32_t>(dma_buffer_);
  DMA1_Channel6->CNDTR = len;
  DMA1->IFCR = DMA_IFCR_CGIF6 | DMA_IFCR_CTCIF6 | DMA_IFCR_CHTIF6 | DMA_IFCR_CTEIF6;
  DMA1_Channel6->CCR = DMA_CCR_DIR | DMA_CCR_MINC | DMA_CCR_PSIZE_0 |
                       DMA_CCR_MSIZE_0 | DMA_CCR_PL_1 | DMA_CCR_EN;

  TIM3->CNT = 0u;
  TIM3->CR1 |= TIM_CR1_CEN;
  while ((DMA1->ISR & DMA_ISR_TCIF6) == 0u)
  {
    if (static_cast<uint32_t>(HAL_GetTick() - start) > 3u)
    {
      break;
    }
  }

  TIM3->CR1 &= ~TIM_CR1_CEN;
  DMA1_Channel6->CCR &= ~DMA_CCR_EN;
  DMA1->IFCR = DMA_IFCR_CGIF6 | DMA_IFCR_CTCIF6 | DMA_IFCR_CHTIF6 | DMA_IFCR_CTEIF6;
  TIM3->CCR1 = 0u;
}

}  // namespace app
