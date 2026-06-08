#include "SensorNode.hpp"

#include "App/BoardPins.hpp"
#include "App/Config.hpp"
#include "App/Hal/Hardware.hpp"
#include "App/Protocol/FrameCodec.hpp"

#include "main.h"

#include <stdio.h>

namespace app {
namespace {

struct NtcTablePoint
{
  uint16_t adc;
  int16_t c10;
};

constexpr NtcTablePoint ntc_table[] =
{
  {128u, 1293},
  {384u, 866},
  {640u, 685},
  {896u, 567},
  {1152u, 477},
  {1408u, 403},
  {1664u, 338},
  {1920u, 278},
  {2176u, 222},
  {2432u, 167},
  {2688u, 111},
  {2944u, 53},
  {3200u, -11},
  {3456u, -87},
  {3712u, -186},
  {3968u, -364},
};

}  // namespace

void SensorNode::init()
{
  initGpio();
  hal::initAdc1();

  last_sensor_ms_ = 0u;
  last_dht_ms_ = 0u - dht11_period_ms;
  seq_ = 0u;
  temp_ = 0u;
  humi_ = 0u;
  dht_ok_ = 0u;
  avg_valid_ = 0u;
  mq135_avg_ = 0u;
  mq2_avg_ = 0u;
  rain_avg_ = 0u;
  therm_avg_ = 0u;

  printf("\r\n[SENSOR] boot, USART1 debug ready, USART3 link ready\r\n");
}

void SensorNode::initGpio()
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  gpio.Pin = GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
  gpio.Mode = GPIO_MODE_ANALOG;
  HAL_GPIO_Init(GPIOA, &gpio);

  gpio.Pin = pins::flame_pin;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(pins::flame_port, &gpio);

  gpio.Pin = pins::therm_do_pin;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(pins::therm_do_port, &gpio);

  dht_.initGpio();
}

void SensorNode::run()
{
  while (1)
  {
    const uint32_t now = HAL_GetTick();
    if (static_cast<uint32_t>(now - last_sensor_ms_) >= sensor_period_ms)
    {
      SensorFrame frame;
      const uint16_t mq135_raw = hal::readAdc1Channel(4u);
      const uint16_t mq2_raw = hal::readAdc1Channel(5u);
      const uint16_t rain_raw = hal::readAdc1Channel(6u);
      const uint16_t therm_raw = hal::readAdc1Channel(7u);
      bool therm_adc_ok = false;
      const uint8_t therm_do_hot =
        (HAL_GPIO_ReadPin(pins::therm_do_port, pins::therm_do_pin) == GPIO_PIN_RESET) ? 1u : 0u;

      if (static_cast<uint32_t>(now - last_dht_ms_) >= dht11_period_ms)
      {
        dht_ok_ = dht_.read(temp_, humi_) ? 1u : 0u;
        last_dht_ms_ = now;
      }

      if (avg_valid_ == 0u)
      {
        mq135_avg_ = mq135_raw;
        mq2_avg_ = mq2_raw;
        rain_avg_ = rain_raw;
        therm_avg_ = therm_raw;
        avg_valid_ = 1u;
      }
      else
      {
        mq135_avg_ = filter(mq135_avg_, mq135_raw, avg_valid_ != 0u);
        mq2_avg_ = filter(mq2_avg_, mq2_raw, avg_valid_ != 0u);
        rain_avg_ = filter(rain_avg_, rain_raw, avg_valid_ != 0u);
        therm_avg_ = filter(therm_avg_, therm_raw, avg_valid_ != 0u);
      }

      frame.temp = temp_;
      frame.humi = humi_;
      frame.mq135_adc = mq135_avg_;
      frame.mq2_adc = mq2_avg_;
      frame.rain_adc = rain_avg_;
      frame.therm_adc = therm_avg_;
      frame.therm_c10 = thermistorAdcToC10(therm_avg_, therm_adc_ok);
      frame.flame = (HAL_GPIO_ReadPin(pins::flame_port, pins::flame_pin) == GPIO_PIN_RESET) ? 1u : 0u;
      frame.rain_wet = (rain_avg_ >= rain_wet_adc_default) ? 1u : 0u;
      frame.therm_hot = therm_do_hot;
      frame.seq = seq_++;
      frame.status = dht_ok_ ? 0u : sensor_status(SensorStatus::DhtError);
      frame.status |= therm_do_hot ? sensor_status(SensorStatus::ThermHotDigital) : 0u;
      frame.status |= frame.rain_wet ? sensor_status(SensorStatus::RainWet) : 0u;
      frame.status |= therm_adc_ok ? 0u : sensor_status(SensorStatus::ThermAdcError);

      sendFrame(frame);
      printf("[SENSOR] seq=%u t=%u h=%u mq135=%u mq2=%u rain=%u therm=%d.%dC flame=%u status=0x%02X\r\n",
             frame.seq, frame.temp, frame.humi, frame.mq135_adc, frame.mq2_adc,
             frame.rain_adc, frame.therm_c10 / 10,
             frame.therm_c10 < 0 ? -(frame.therm_c10 % 10) : (frame.therm_c10 % 10),
             frame.flame, frame.status);

      last_sensor_ms_ = now;
    }
  }
}

uint16_t SensorNode::filter(uint16_t previous, uint16_t sample, bool valid)
{
  if (!valid)
  {
    return sample;
  }
  return static_cast<uint16_t>((static_cast<uint32_t>(previous) * 3u + sample) / 4u);
}

int16_t SensorNode::thermistorAdcToC10(uint16_t adc, bool &valid)
{
  constexpr uint8_t table_count = static_cast<uint8_t>(sizeof(ntc_table) / sizeof(ntc_table[0]));

  if ((adc <= 8u) || (adc >= 4088u))
  {
    valid = false;
    return 0;
  }

  valid = true;
  if (adc <= ntc_table[0].adc)
  {
    return ntc_table[0].c10;
  }
  if (adc >= ntc_table[table_count - 1u].adc)
  {
    return ntc_table[table_count - 1u].c10;
  }

  for (uint8_t i = 0u; i < static_cast<uint8_t>(table_count - 1u); i++)
  {
    const NtcTablePoint &lo = ntc_table[i];
    const NtcTablePoint &hi = ntc_table[i + 1u];
    if ((adc >= lo.adc) && (adc <= hi.adc))
    {
      const int32_t adc_span = static_cast<int32_t>(hi.adc) - static_cast<int32_t>(lo.adc);
      const int32_t temp_span = static_cast<int32_t>(hi.c10) - static_cast<int32_t>(lo.c10);
      const int32_t offset = static_cast<int32_t>(adc) - static_cast<int32_t>(lo.adc);
      return static_cast<int16_t>(static_cast<int32_t>(lo.c10) + ((temp_span * offset) / adc_span));
    }
  }

  return 0;
}

void SensorNode::sendFrame(const SensorFrame &frame)
{
  uint8_t bytes[FrameCodec::total_len];
  const uint8_t len = FrameCodec::encode(frame, bytes);
  hal::sendUsartBuffer(USART3, bytes, len);
}

}  // namespace app
