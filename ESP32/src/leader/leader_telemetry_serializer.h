#pragma once

#include "leader_telemetry_state.h"

#include <cstdint>

namespace soarm {

class LeaderTelemetrySerializer {
public:
  static constexpr uint16_t kMagic = 0x5341;
  static constexpr uint8_t kVersion = 1;
  static constexpr uint8_t kTelemetryType = 1;

  struct Packet {
    uint16_t magic;
    uint8_t version;
    uint8_t packetType;
    LeaderTelemetrySnapshot payload;
  } __attribute__((packed));

  Packet serialize(const LeaderTelemetrySnapshot &snapshot) const;
};

} // namespace soarm
