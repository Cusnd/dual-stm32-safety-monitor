#include "W25q64FlashLogger.hpp"

#include "App/BoardPins.hpp"
#include "App/Protocol/FrameCodec.hpp"

#include "main.h"
#include "spi.h"

#include <stdio.h>
#include <string.h>

namespace app {
namespace {

uint16_t packThresholdLevels(const ThresholdLevels &levels, bool muted)
{
  return static_cast<uint16_t>(
    (normalizeThresholdLevel(levels.air) & 0x07u) |
    ((normalizeThresholdLevel(levels.smoke) & 0x07u) << 3) |
    ((normalizeThresholdLevel(levels.rain) & 0x07u) << 6) |
    ((normalizeThresholdLevel(levels.therm) & 0x07u) << 9) |
    (muted ? 0x1000u : 0u));
}

}  // namespace

void W25q64FlashLogger::init()
{
  GPIO_InitTypeDef gpio = {0};
  uint8_t manufacturer;
  uint8_t memory_type;
  uint8_t capacity;

  __HAL_RCC_GPIOB_CLK_ENABLE();
  MX_SPI2_Init();

  gpio.Pin = pins::flash_cs_pin;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(pins::flash_cs_port, &gpio);
  cs(true);

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
  task_ = Task::None;
  task_start_ms_ = 0u;
  task_timeout_ms_ = 0u;
  pending_log_addr_ = log_start_addr;
  pending_log_ = 0u;
  pending_log_needs_erase_ = 0u;
  pending_metadata_ = 0u;
  active_log_addr_ = 0u;
  active_log_count_ = 0u;
  active_log_seq_ = 0u;
  active_log_state_ = 0u;

  if (present_ != 0u)
  {
    if (!loadMetadata())
    {
      log_addr_ = log_start_addr;
      record_count_ = 0u;
      meta_addr_ = sector_size;
      scheduleMetadata();
    }

    printf("[MONITOR] W25Q ID %02X %02X %02X, cursor=0x%06lX records=%lu\r\n",
           manufacturer, memory_type, capacity,
           static_cast<unsigned long>(log_addr_),
           static_cast<unsigned long>(record_count_));
  }
}

bool W25q64FlashLogger::logFrame(
  const SensorFrame &frame,
  AlarmState state,
  const ThresholdLevels &levels,
  bool muted)
{
  const uint32_t tick = HAL_GetTick();
  const uint16_t crc_len = static_cast<uint16_t>(log_record_size - 2u);
  const uint16_t packed_thresholds = packThresholdLevels(levels, muted);
  uint16_t crc;

  if ((present_ == 0u) || (pending_log_ != 0u))
  {
    return false;
  }

  if ((log_addr_ < log_start_addr) ||
      (log_addr_ + log_record_size > log_end_addr) ||
      (((log_addr_ - log_start_addr) % log_record_size) != 0u))
  {
    log_addr_ = log_start_addr;
  }

  memset(pending_record_, 0xFF, sizeof(pending_record_));
  pending_record_[0] = record_magic;
  pending_record_[1] = FrameCodec::version;
  pending_record_[2] = frame.seq;
  pending_record_[3] = frame.status;
  pending_record_[4] = frame.temp;
  pending_record_[5] = frame.humi;
  pending_record_[6] = static_cast<uint8_t>(frame.mq135_adc >> 8);
  pending_record_[7] = static_cast<uint8_t>(frame.mq135_adc);
  pending_record_[8] = static_cast<uint8_t>(frame.mq2_adc >> 8);
  pending_record_[9] = static_cast<uint8_t>(frame.mq2_adc);
  pending_record_[10] = static_cast<uint8_t>(frame.rain_adc >> 8);
  pending_record_[11] = static_cast<uint8_t>(frame.rain_adc);
  pending_record_[12] = static_cast<uint8_t>(frame.therm_adc >> 8);
  pending_record_[13] = static_cast<uint8_t>(frame.therm_adc);
  pending_record_[14] = static_cast<uint8_t>(static_cast<uint16_t>(frame.therm_c10) >> 8);
  pending_record_[15] = static_cast<uint8_t>(static_cast<uint16_t>(frame.therm_c10));
  pending_record_[16] = frame.flame;
  pending_record_[17] = frame.rain_wet;
  pending_record_[18] = frame.therm_hot;
  pending_record_[19] = static_cast<uint8_t>(state);
  pending_record_[20] = static_cast<uint8_t>(tick >> 24);
  pending_record_[21] = static_cast<uint8_t>(tick >> 16);
  pending_record_[22] = static_cast<uint8_t>(tick >> 8);
  pending_record_[23] = static_cast<uint8_t>(tick);
  pending_record_[24] = static_cast<uint8_t>(packed_thresholds >> 8);
  pending_record_[25] = static_cast<uint8_t>(packed_thresholds);
  u32ToBytes(&pending_record_[26], record_count_);

  crc = crc16(pending_record_, static_cast<uint8_t>(crc_len));
  pending_record_[30] = static_cast<uint8_t>(crc >> 8);
  pending_record_[31] = static_cast<uint8_t>(crc);

  pending_log_addr_ = log_addr_;
  pending_log_needs_erase_ = (((log_addr_ - log_start_addr) % sector_size) == 0u) ? 1u : 0u;
  pending_log_ = 1u;
  return true;
}

void W25q64FlashLogger::process()
{
  if (present_ == 0u)
  {
    return;
  }

  if (task_ != Task::None)
  {
    if (flashBusy())
    {
      if (static_cast<uint32_t>(HAL_GetTick() - task_start_ms_) > task_timeout_ms_)
      {
        failTask();
      }
      return;
    }

    completeTask();
  }

  if (task_ != Task::None)
  {
    return;
  }

  if (pending_log_ != 0u)
  {
    if (pending_log_needs_erase_ != 0u)
    {
      startSectorErase(pending_log_addr_, Task::EraseLogSector, 1000u);
      return;
    }

    active_log_addr_ = pending_log_addr_;
    active_log_count_ = bytesToU32(&pending_record_[26]);
    active_log_seq_ = pending_record_[2];
    active_log_state_ = pending_record_[19];
    startPageProgram(pending_log_addr_, pending_record_,
                     static_cast<uint8_t>(sizeof(pending_record_)),
                     Task::ProgramLogRecord, 10u);
    return;
  }

  if (pending_metadata_ != 0u)
  {
    if (meta_addr_ + meta_entry_size > sector_size)
    {
      startSectorErase(0u, Task::EraseMetadataSector, 1000u);
      return;
    }

    startPageProgram(meta_addr_, metadata_record_,
                     static_cast<uint8_t>(sizeof(metadata_record_)),
                     Task::ProgramMetadata, 10u);
  }
}

bool W25q64FlashLogger::present() const
{
  return present_ != 0u;
}

bool W25q64FlashLogger::busy() const
{
  return (task_ != Task::None) || (pending_log_ != 0u) || (pending_metadata_ != 0u);
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
  uint8_t rx = 0xFFu;

  if (HAL_SPI_TransmitReceive(&hspi2, &data, &rx, 1u, 10u) != HAL_OK)
  {
    return 0xFFu;
  }

  return rx;
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

bool W25q64FlashLogger::flashBusy()
{
  return (readStatus() & 0x01u) != 0u;
}

void W25q64FlashLogger::writeEnable()
{
  cs(false);
  txRx(0x06u);
  cs(true);
}

void W25q64FlashLogger::startSectorErase(uint32_t addr, Task task, uint32_t timeout_ms)
{
  writeEnable();
  cs(false);
  txRx(0x20u);
  txRx(static_cast<uint8_t>(addr >> 16));
  txRx(static_cast<uint8_t>(addr >> 8));
  txRx(static_cast<uint8_t>(addr));
  cs(true);

  task_ = task;
  task_start_ms_ = HAL_GetTick();
  task_timeout_ms_ = timeout_ms;
}

void W25q64FlashLogger::startPageProgram(
  uint32_t addr,
  const uint8_t *data,
  uint8_t len,
  Task task,
  uint32_t timeout_ms)
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

  task_ = task;
  task_start_ms_ = HAL_GetTick();
  task_timeout_ms_ = timeout_ms;
}

void W25q64FlashLogger::completeTask()
{
  const Task completed = task_;
  task_ = Task::None;

  switch (completed)
  {
    case Task::EraseLogSector:
      pending_log_needs_erase_ = 0u;
      break;

    case Task::ProgramLogRecord:
      printf("[MONITOR] flash log addr=0x%06lX count=%lu seq=%u state=%u\r\n",
             static_cast<unsigned long>(active_log_addr_),
             static_cast<unsigned long>(active_log_count_),
             active_log_seq_,
             static_cast<unsigned int>(active_log_state_));
      pending_log_ = 0u;
      log_addr_ = pending_log_addr_ + log_record_size;
      record_count_++;
      if (log_addr_ >= log_end_addr)
      {
        log_addr_ = log_start_addr;
      }
      scheduleMetadata();
      break;

    case Task::EraseMetadataSector:
      meta_addr_ = 0u;
      break;

    case Task::ProgramMetadata:
      pending_metadata_ = 0u;
      meta_addr_ += meta_entry_size;
      break;

    case Task::None:
    default:
      break;
  }
}

void W25q64FlashLogger::failTask()
{
  task_ = Task::None;
  pending_log_ = 0u;
  pending_metadata_ = 0u;
  present_ = 0u;
  printf("[MONITOR] flash timeout, logging disabled\r\n");
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

void W25q64FlashLogger::scheduleMetadata()
{
  uint16_t crc;

  memset(metadata_record_, 0xFF, sizeof(metadata_record_));
  metadata_record_[0] = meta_magic0;
  metadata_record_[1] = meta_magic1;
  metadata_record_[2] = FrameCodec::version;
  metadata_record_[3] = 0u;
  u32ToBytes(&metadata_record_[4], log_addr_);
  u32ToBytes(&metadata_record_[8], record_count_);
  crc = crc16(metadata_record_, 12u);
  metadata_record_[12] = static_cast<uint8_t>(crc >> 8);
  metadata_record_[13] = static_cast<uint8_t>(crc);
  pending_metadata_ = 1u;
}

}  // namespace app
