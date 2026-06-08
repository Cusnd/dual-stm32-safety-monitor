#pragma once

#include "App/Config.hpp"
#include "App/Protocol/SensorFrame.hpp"

#include <stdint.h>

namespace app {

class W25q64FlashLogger
{
public:
  void init();
  void logFrame(const SensorFrame &frame, AlarmState state, uint8_t threshold_profile, bool muted);
  bool present() const;
  uint32_t recordCount() const;

private:
  static constexpr uint32_t flash_size_bytes = 0x800000u;
  static constexpr uint32_t sector_size = 4096u;
  static constexpr uint8_t meta_entry_size = 16u;
  static constexpr uint8_t meta_magic0 = 0x4Du;
  static constexpr uint8_t meta_magic1 = 0x32u;
  static constexpr uint8_t record_magic = 0xE2u;
  static constexpr uint32_t log_start_addr = sector_size;
  static constexpr uint8_t log_record_size = 32u;
  static constexpr uint32_t log_end_addr = flash_size_bytes;

  void cs(bool high);
  uint8_t txRx(uint8_t data);
  uint8_t readStatus();
  bool waitReady(uint32_t timeout_ms);
  void writeEnable();
  void sectorErase(uint32_t addr);
  void pageProgram(uint32_t addr, const uint8_t *data, uint8_t len);
  void readData(uint32_t addr, uint8_t *data, uint16_t len);
  uint16_t crc16(const uint8_t *data, uint8_t len);
  void u32ToBytes(uint8_t *out, uint32_t value);
  uint32_t bytesToU32(const uint8_t *in);
  bool loadMetadata();
  void writeMetadata();

  uint8_t present_;
  uint32_t log_addr_;
  uint32_t record_count_;
  uint32_t meta_addr_;
};

}  // namespace app
