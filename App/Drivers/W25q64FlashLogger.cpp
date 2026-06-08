#include "W25q64FlashLogger.hpp"

#include "App/BoardPins.hpp"
#include "App/Protocol/FrameCodec.hpp"

#include "main.h"

#include <stdio.h>
#include <string.h>

namespace app {

void W25q64FlashLogger::init()
{
  GPIO_InitTypeDef gpio = {0};
  uint8_t manufacturer;
  uint8_t memory_type;
  uint8_t capacity;

  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_SPI2_CLK_ENABLE();

  gpio.Pin = GPIO_PIN_13 | GPIO_PIN_15;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &gpio);

  gpio.Pin = GPIO_PIN_14;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOB, &gpio);

  gpio.Pin = pins::flash_cs_pin;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(pins::flash_cs_port, &gpio);
  cs(true);

  SPI2->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_BR_1 | SPI_CR1_BR_0;
  SPI2->CR1 |= SPI_CR1_SPE;

  cs(false);
  txRx(0x9Fu);
  manufacturer = txRx(0xFFu);
  memory_type = txRx(0xFFu);
  capacity = txRx(0xFFu);
  cs(true);

  present_ = (((manufacturer == 0xEFu) || (manufacturer == 0xC8u)) &&
              ((memory_type == 0x40u) || (memory_type == 0x60u)) &&
              (capacity == 0x17u)) ? 1u : 0u;
  log_addr_ = log_start_addr;
  record_count_ = 0u;
  meta_addr_ = 0u;

  if (present_ != 0u)
  {
    if (!loadMetadata())
    {
      log_addr_ = log_start_addr;
      record_count_ = 0u;
      meta_addr_ = sector_size;
      writeMetadata();
    }

    printf("[MONITOR] W25Q ID %02X %02X %02X, cursor=0x%06lX records=%lu\r\n",
           manufacturer, memory_type, capacity,
           static_cast<unsigned long>(log_addr_),
           static_cast<unsigned long>(record_count_));
  }
}

void W25q64FlashLogger::logFrame(
  const SensorFrame &frame,
  AlarmState state,
  uint8_t threshold_profile,
  bool muted)
{
  uint8_t record[log_record_size];
  const uint32_t tick = HAL_GetTick();
  const uint16_t crc_len = static_cast<uint16_t>(log_record_size - 2u);
  uint16_t crc;

  if (present_ == 0u)
  {
    return;
  }

  if ((log_addr_ < log_start_addr) ||
      (log_addr_ + log_record_size > log_end_addr) ||
      (((log_addr_ - log_start_addr) % log_record_size) != 0u))
  {
    log_addr_ = log_start_addr;
  }

  if (((log_addr_ - log_start_addr) % sector_size) == 0u)
  {
    sectorErase(log_addr_);
  }

  memset(record, 0xFF, sizeof(record));
  record[0] = record_magic;
  record[1] = FrameCodec::version;
  record[2] = frame.seq;
  record[3] = frame.status;
  record[4] = frame.temp;
  record[5] = frame.humi;
  record[6] = static_cast<uint8_t>(frame.mq135_adc >> 8);
  record[7] = static_cast<uint8_t>(frame.mq135_adc);
  record[8] = static_cast<uint8_t>(frame.mq2_adc >> 8);
  record[9] = static_cast<uint8_t>(frame.mq2_adc);
  record[10] = static_cast<uint8_t>(frame.rain_adc >> 8);
  record[11] = static_cast<uint8_t>(frame.rain_adc);
  record[12] = static_cast<uint8_t>(frame.therm_adc >> 8);
  record[13] = static_cast<uint8_t>(frame.therm_adc);
  record[14] = static_cast<uint8_t>(static_cast<uint16_t>(frame.therm_c10) >> 8);
  record[15] = static_cast<uint8_t>(static_cast<uint16_t>(frame.therm_c10));
  record[16] = frame.flame;
  record[17] = frame.rain_wet;
  record[18] = frame.therm_hot;
  record[19] = static_cast<uint8_t>(state);
  record[20] = static_cast<uint8_t>(tick >> 24);
  record[21] = static_cast<uint8_t>(tick >> 16);
  record[22] = static_cast<uint8_t>(tick >> 8);
  record[23] = static_cast<uint8_t>(tick);
  record[24] = threshold_profile;
  record[25] = muted ? 1u : 0u;
  u32ToBytes(&record[26], record_count_);

  crc = crc16(record, static_cast<uint8_t>(crc_len));
  record[30] = static_cast<uint8_t>(crc >> 8);
  record[31] = static_cast<uint8_t>(crc);

  pageProgram(log_addr_, record, static_cast<uint8_t>(sizeof(record)));
  printf("[MONITOR] flash log addr=0x%06lX count=%lu seq=%u state=%u\r\n",
         static_cast<unsigned long>(log_addr_),
         static_cast<unsigned long>(record_count_),
         frame.seq,
         static_cast<unsigned int>(state));

  log_addr_ += log_record_size;
  record_count_++;
  if (log_addr_ >= log_end_addr)
  {
    log_addr_ = log_start_addr;
  }
  writeMetadata();
}

bool W25q64FlashLogger::present() const
{
  return present_ != 0u;
}

uint32_t W25q64FlashLogger::recordCount() const
{
  return record_count_;
}

void W25q64FlashLogger::cs(bool high)
{
  HAL_GPIO_WritePin(pins::flash_cs_port, pins::flash_cs_pin, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

uint8_t W25q64FlashLogger::txRx(uint8_t data)
{
  while ((SPI2->SR & SPI_SR_TXE) == 0u)
  {
  }
  *reinterpret_cast<__IO uint8_t *>(&SPI2->DR) = data;
  while ((SPI2->SR & SPI_SR_RXNE) == 0u)
  {
  }
  return static_cast<uint8_t>(SPI2->DR);
}

uint8_t W25q64FlashLogger::readStatus()
{
  uint8_t status;
  cs(false);
  txRx(0x05u);
  status = txRx(0xFFu);
  cs(true);
  return status;
}

bool W25q64FlashLogger::waitReady(uint32_t timeout_ms)
{
  const uint32_t start = HAL_GetTick();
  while ((readStatus() & 0x01u) != 0u)
  {
    if (static_cast<uint32_t>(HAL_GetTick() - start) > timeout_ms)
    {
      return false;
    }
  }
  return true;
}

void W25q64FlashLogger::writeEnable()
{
  cs(false);
  txRx(0x06u);
  cs(true);
}

void W25q64FlashLogger::sectorErase(uint32_t addr)
{
  writeEnable();
  cs(false);
  txRx(0x20u);
  txRx(static_cast<uint8_t>(addr >> 16));
  txRx(static_cast<uint8_t>(addr >> 8));
  txRx(static_cast<uint8_t>(addr));
  cs(true);
  (void)waitReady(1000u);
}

void W25q64FlashLogger::pageProgram(uint32_t addr, const uint8_t *data, uint8_t len)
{
  writeEnable();
  cs(false);
  txRx(0x02u);
  txRx(static_cast<uint8_t>(addr >> 16));
  txRx(static_cast<uint8_t>(addr >> 8));
  txRx(static_cast<uint8_t>(addr));
  for (uint8_t i = 0u; i < len; i++)
  {
    txRx(data[i]);
  }
  cs(true);
  (void)waitReady(10u);
}

void W25q64FlashLogger::readData(uint32_t addr, uint8_t *data, uint16_t len)
{
  cs(false);
  txRx(0x03u);
  txRx(static_cast<uint8_t>(addr >> 16));
  txRx(static_cast<uint8_t>(addr >> 8));
  txRx(static_cast<uint8_t>(addr));
  for (uint16_t i = 0u; i < len; i++)
  {
    data[i] = txRx(0xFFu);
  }
  cs(true);
}

uint16_t W25q64FlashLogger::crc16(const uint8_t *data, uint8_t len)
{
  uint16_t crc = 0xFFFFu;

  for (uint8_t i = 0u; i < len; i++)
  {
    crc ^= data[i];
    for (uint8_t bit = 0u; bit < 8u; bit++)
    {
      crc = (crc & 0x0001u) ? static_cast<uint16_t>((crc >> 1) ^ 0xA001u) :
                              static_cast<uint16_t>(crc >> 1);
    }
  }

  return crc;
}

void W25q64FlashLogger::u32ToBytes(uint8_t *out, uint32_t value)
{
  out[0] = static_cast<uint8_t>(value >> 24);
  out[1] = static_cast<uint8_t>(value >> 16);
  out[2] = static_cast<uint8_t>(value >> 8);
  out[3] = static_cast<uint8_t>(value);
}

uint32_t W25q64FlashLogger::bytesToU32(const uint8_t *in)
{
  return (static_cast<uint32_t>(in[0]) << 24) |
         (static_cast<uint32_t>(in[1]) << 16) |
         (static_cast<uint32_t>(in[2]) << 8) |
         static_cast<uint32_t>(in[3]);
}

bool W25q64FlashLogger::loadMetadata()
{
  uint8_t entry[meta_entry_size];
  bool found = false;
  uint32_t next_meta = 0u;

  for (uint32_t addr = 0u; addr < sector_size; addr += meta_entry_size)
  {
    bool blank = true;
    readData(addr, entry, static_cast<uint16_t>(sizeof(entry)));
    for (uint8_t i = 0u; i < sizeof(entry); i++)
    {
      if (entry[i] != 0xFFu)
      {
        blank = false;
        break;
      }
    }

    if (blank)
    {
      next_meta = addr;
      break;
    }

    if ((entry[0] == meta_magic0) && (entry[1] == meta_magic1) &&
        (entry[2] == FrameCodec::version))
    {
      const uint16_t stored_crc = static_cast<uint16_t>((static_cast<uint16_t>(entry[12]) << 8) | entry[13]);
      const uint16_t calc_crc = crc16(entry, 12u);
      const uint32_t log_addr = bytesToU32(&entry[4]);
      const uint32_t count = bytesToU32(&entry[8]);

      if ((stored_crc == calc_crc) &&
          (log_addr >= log_start_addr) &&
          (log_addr < log_end_addr) &&
          (((log_addr - log_start_addr) % log_record_size) == 0u))
      {
        log_addr_ = log_addr;
        record_count_ = count;
        found = true;
      }
    }

    next_meta = addr + meta_entry_size;
  }

  if (next_meta >= sector_size)
  {
    next_meta = sector_size;
  }
  meta_addr_ = next_meta;

  return found;
}

void W25q64FlashLogger::writeMetadata()
{
  uint8_t entry[meta_entry_size];
  uint16_t crc;

  if (meta_addr_ + meta_entry_size > sector_size)
  {
    sectorErase(0u);
    meta_addr_ = 0u;
  }

  memset(entry, 0xFF, sizeof(entry));
  entry[0] = meta_magic0;
  entry[1] = meta_magic1;
  entry[2] = FrameCodec::version;
  entry[3] = 0u;
  u32ToBytes(&entry[4], log_addr_);
  u32ToBytes(&entry[8], record_count_);
  crc = crc16(entry, 12u);
  entry[12] = static_cast<uint8_t>(crc >> 8);
  entry[13] = static_cast<uint8_t>(crc);

  pageProgram(meta_addr_, entry, static_cast<uint8_t>(sizeof(entry)));
  meta_addr_ += meta_entry_size;
}

}  // namespace app
