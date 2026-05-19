#pragma once

#include <cstdint>

namespace soarm {

struct PresencePacket {
  uint8_t magic;
  uint8_t version;
  uint8_t messageType;
  uint8_t reserved;
  char ip[16];
} __attribute__((packed));

} // namespace soarm
