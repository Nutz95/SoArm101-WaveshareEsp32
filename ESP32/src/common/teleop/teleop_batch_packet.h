#pragma once

#include <cstdint>

namespace soarm {
namespace teleop_batch {

constexpr uint16_t kMagic = 0x5457U;
constexpr uint8_t kVersion = 2U;
constexpr uint8_t kTypeBatch = 1U;
constexpr uint8_t kTypeAck = 2U;

struct BatchEntry {
  uint8_t id;
  int16_t position;
} __attribute__((packed));

struct BatchPacket {
  uint16_t magic;
  uint8_t version;
  uint8_t flags;
  uint8_t type;
  uint16_t requestId;
  uint8_t count;
  uint8_t speedPercent;
  BatchEntry entries[6];
} __attribute__((packed));

struct AckPacket {
  uint16_t magic;
  uint8_t version;
  uint8_t type;
  uint16_t requestId;
  uint8_t status;
} __attribute__((packed));

} // namespace teleop_batch
} // namespace soarm
