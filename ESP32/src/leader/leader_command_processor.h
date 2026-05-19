#pragma once

#include "../common/lock_manager.h"

#include <cstdint>

namespace soarm {

enum class LeaderCommandAction : uint8_t {
  None = 0,
  StartStream,
  StopStream,
  Ping,
  ResetPairing,
  ServoScan,
  ServoDebugEnable,
  ServoDebugDisable,
  ServoMove
};

class LeaderCommandProcessor {
public:
  static constexpr uint16_t kMagic = 0x5343;
  static constexpr uint8_t kVersion = 1;

  struct CommandFrame {
    uint16_t magic;
    uint8_t version;
    uint8_t command;
    uint32_t value;
  } __attribute__((packed));

  LeaderCommandProcessor();

  LeaderCommandAction process(const CommandFrame &frame);

private:
  LockManager lockManager_;
};

} // namespace soarm
