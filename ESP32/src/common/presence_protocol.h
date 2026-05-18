#pragma once

#include <cstdint>

namespace soarm {

constexpr uint8_t kPresenceMagic = 0xA5;
constexpr uint8_t kPresenceVersion = 1;
constexpr uint32_t kPresenceTxPeriodMs = 1000U;
constexpr uint32_t kPresenceTimeoutMs = 3500U;

enum class PresenceMessageType : uint8_t {
  PairRequest = 1,
  PairAck = 2,
  Presence = 3
};

struct PresencePacket {
  uint8_t magic;
  uint8_t version;
  uint8_t messageType;
  uint8_t reserved;
  char ip[16];
};

} // namespace soarm
