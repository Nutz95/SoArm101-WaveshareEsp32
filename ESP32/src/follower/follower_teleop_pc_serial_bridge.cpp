#include "follower_teleop_pc_serial_bridge.h"

#include "../Config/common_runtime_config.h"
#include "../common/teleop/teleop_batch_packet.h"

namespace soarm {

void FollowerTeleopPcSerialBridge::attach(HardwareSerial &serial) {
  serial_ = &serial;
}

bool FollowerTeleopPcSerialBridge::consumeBatch(
    uint8_t *ids,
    int16_t *positions,
    uint8_t capacity,
    uint8_t &count,
    uint8_t &speedPercent,
    uint16_t &requestId,
    uint8_t &flags) {
  if (serial_ == nullptr || ids == nullptr || positions == nullptr || capacity == 0U) {
    return false;
  }

  constexpr uint8_t kMagicLo = static_cast<uint8_t>(teleop_batch::kMagic & 0xFFU);
  while (serial_->available() > 0) {
    if (static_cast<uint8_t>(serial_->peek()) != kMagicLo) {
      serial_->read();
      continue;
    }
    if (serial_->available() < static_cast<int>(sizeof(teleop_batch::BatchPacket))) {
      return false;
    }
    break;
  }

  if (serial_->available() < static_cast<int>(sizeof(teleop_batch::BatchPacket))) {
    return false;
  }

  teleop_batch::BatchPacket packet{};
  const size_t readLen = serial_->readBytes(reinterpret_cast<uint8_t *>(&packet), sizeof(packet));
  if (readLen != sizeof(packet)) {
    return false;
  }

  if (packet.magic != teleop_batch::kMagic ||
      packet.version != teleop_batch::kVersion ||
      packet.type != teleop_batch::kTypeBatch) {
    return false;
  }

  const uint8_t copyCount = (packet.count < capacity) ? packet.count : capacity;
  for (uint8_t i = 0U; i < copyCount; ++i) {
    ids[i] = packet.entries[i].id;
    positions[i] = packet.entries[i].position;
  }

  count = copyCount;
  speedPercent = packet.speedPercent;
  requestId = packet.requestId;
  flags = packet.flags;
  return true;
}

bool FollowerTeleopPcSerialBridge::drainLatestBatch(
    uint8_t *ids,
    int16_t *positions,
    uint8_t capacity,
    uint8_t &count,
    uint8_t &speedPercent,
    uint16_t &requestId,
    uint8_t &flags) {
  bool got = false;
  while (consumeBatch(ids, positions, capacity, count, speedPercent, requestId, flags)) {
    got = true;
  }
  return got;
}

} // namespace soarm
