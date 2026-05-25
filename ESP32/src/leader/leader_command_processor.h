#pragma once

#include "../common/lock_manager.h"

#include <cstdint>

namespace soarm {

enum class LeaderCommandAction : uint8_t {
  None = 0,
  StartStream = 1,
  StopStream = 2,
  Ping = 3,
  ResetPairing = 4,
  ServoScan = 5,
  ServoDebugEnable = 6,
  ServoDebugDisable = 7,
  ServoMove = 8,
  ServoSetId = 9,
  ServoSetMode = 10,
  ServoDebugEnableFollower = 11,
  ServoDebugDisableFollower = 12,
  ServoScanLeader = 13,
  ServoScanFollower = 14,
  TeleopMirror = 15
};

class LeaderCommandProcessor {
public:
  static constexpr uint16_t kMagic = 0x5343;
  static constexpr uint8_t kVersion = 1;

  struct CommandFrame {
    uint16_t magic;
    uint8_t version;
    uint8_t command;
    uint16_t requestId;
    uint32_t value;
  } __attribute__((packed));

  LeaderCommandProcessor();

  LeaderCommandAction process(const CommandFrame &frame);

private:
  LockManager lockManager_;
};

} // namespace soarm
