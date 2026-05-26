#include "follower_teleop_wifi_bridge.h"

namespace soarm {

bool FollowerTeleopWifiBridge::begin(uint16_t localPort) {
  started_ = udp_.begin(localPort) == 1;
  return started_;
}

bool FollowerTeleopWifiBridge::consumeBatch(
    uint8_t *ids,
    int16_t *positions,
    uint8_t capacity,
    uint8_t &count,
    uint8_t &speedPct,
    uint16_t &requestId) {
  if (!started_ || ids == nullptr || positions == nullptr || capacity == 0U) {
    return false;
  }

  const int packetSize = udp_.parsePacket();
  if (packetSize < static_cast<int>(sizeof(teleop_wifi::BatchPacket))) {
    return false;
  }

  teleop_wifi::BatchPacket packet{};
  const int readLen = udp_.read(reinterpret_cast<uint8_t *>(&packet), sizeof(packet));
  if (readLen != static_cast<int>(sizeof(packet))) {
    return false;
  }

  if (packet.magic != teleop_wifi::kMagic ||
      packet.version != teleop_wifi::kVersion ||
      packet.type != teleop_wifi::kTypeBatch) {
    return false;
  }

  const uint8_t copyCount = (packet.count < capacity) ? packet.count : capacity;
  for (uint8_t i = 0U; i < copyCount; ++i) {
    ids[i] = packet.entries[i].id;
    positions[i] = packet.entries[i].position;
  }

  count = copyCount;
  speedPct = packet.speedPct;
  requestId = packet.requestId;

  leaderAckIp_ = udp_.remoteIP();
  leaderAckPort_ = udp_.remotePort();
  hasAckPeer_ = true;
  return true;
}

bool FollowerTeleopWifiBridge::sendAck(uint16_t requestId, uint8_t status) {
  if (!started_ || !hasAckPeer_ || leaderAckPort_ == 0U) {
    return false;
  }

  teleop_wifi::AckPacket packet{};
  packet.magic = teleop_wifi::kMagic;
  packet.version = teleop_wifi::kVersion;
  packet.type = teleop_wifi::kTypeAck;
  packet.requestId = requestId;
  packet.status = status;

  if (udp_.beginPacket(leaderAckIp_, leaderAckPort_) != 1) {
    return false;
  }

  const size_t written = udp_.write(reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
  if (written != sizeof(packet)) {
    udp_.endPacket();
    return false;
  }

  return udp_.endPacket() == 1;
}

} // namespace soarm
