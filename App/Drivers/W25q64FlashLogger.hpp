#pragma once

#include "App/Config.hpp"
#include "App/Protocol/SensorFrame.hpp"

#include <stdint.h>

namespace app {

class W25q64FlashLogger
{
public:
  void init();
  bool logFrame(const SensorFrame &frame, AlarmState state, const ThresholdLevels &levels, bool muted);
  void process();
  bool present() const;
  bool busy() const;
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

  enum class Task : uint8_t
  {
    None,
    EraseLogSector,
    ProgramLogRecord,
    EraseMetadataSector,
    ProgramMetadata,
  };

  void cs(bool high);
  uint8_t txRx(uint8_t data);
  uint8_t readStatus();
  bool flashBusy();
  void startSectorErase(uint32_t addr, Task task, uint32_t timeout_ms);
  void startPageProgram(uint32_t addr, const uint8_t *data, uint8_t len, Task task,
                        uint32_t timeout_ms);
  void completeTask();
  void failTask();
  void writeEnable();
  void readData(uint32_t addr, uint8_t *data, uint16_t len);
  uint16_t crc16(const uint8_t *data, uint8_t len);
  void u32ToBytes(uint8_t *out, uint32_t value);
  uint32_t bytesToU32(const uint8_t *in);
  bool loadMetadata();
  void scheduleMetadata();

  uint8_t present_;
  uint32_t log_addr_;
  uint32_t record_count_;
  uint32_t meta_addr_;
  Task task_;
  uint32_t task_start_ms_;
  uint32_t task_timeout_ms_;
  uint8_t pending_record_[log_record_size];
  uint32_t pending_log_addr_;
  uint8_t pending_log_;
  uint8_t pending_log_needs_erase_;
  uint8_t metadata_record_[meta_entry_size];
  uint8_t pending_metadata_;
  uint32_t active_log_addr_;
  uint32_t active_log_count_;
  uint8_t active_log_seq_;
  uint8_t active_log_state_;
};

}  // namespace app
