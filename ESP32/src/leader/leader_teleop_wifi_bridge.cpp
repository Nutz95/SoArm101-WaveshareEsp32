#include "leader_teleop_wifi_bridge.h"

#include <WiFi.h>

namespace soarm {

bool LeaderTeleopWifiBridge::begin(uint16_t localPort) {
  started_ = udp_.begin(localPort) == 1;
  return started_;
}

bool LeaderTeleopWifiBridge::sendBatch(
    const char *followerIp,
    const uint8_t *ids,
    const int16_t *positions,
    uint8_t count,
  uint8_t speedPercent,
    uint16_t requestId) {
  if (!started_ || followerIp == nullptr || ids == nullptr || positions == nullptr || count == 0U) {
    return false;
  }

  IPAddress remoteIp;
  if (!remoteIp.fromString(followerIp)) {
    return false;
  }

  const uint8_t clampedCount = (count > 6U) ? 6U : count;
  teleop_wifi::BatchPacket packet{};
  packet.magic = teleop_wifi::kMagic;
  packet.version = teleop_wifi::kVersion;
  packet.type = teleop_wifi::kTypeBatch;
  packet.requestId = requestId;
  packet.count = clampedCount;
  packet.speedPercent = speedPercent;
  for (uint8_t i = 0U; i < clampedCount; ++i) {
    packet.entries[i].id = ids[i];
    packet.entries[i].position = positions[i];
  }

  if (udp_.beginPacket(remoteIp, teleop_wifi::kFollowerListenPort) != 1) {
    return false;
  }

  const size_t written = udp_.write(reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
  if (written != sizeof(packet)) {
    udp_.endPacket();
    return false;
  }

  return udp_.endPacket() == 1;
}

bool LeaderTeleopWifiBridge::pollAck(uint16_t &requestId, uint8_t &status) {
  if (!started_) {
    return false;
  }

  const int packetSize = udp_.parsePacket();
  if (packetSize < static_cast<int>(sizeof(teleop_wifi::AckPacket))) {
    return false;
  }

  teleop_wifi::AckPacket ack{};
  const int readLen = udp_.read(reinterpret_cast<uint8_t *>(&ack), sizeof(ack));
  if (readLen != static_cast<int>(sizeof(ack))) {
    return false;
  }

  if (ack.magic != teleop_wifi::kMagic ||
      ack.version != teleop_wifi::kVersion ||
      ack.type != teleop_wifi::kTypeAck) {
    return false;
  }

  requestId = ack.requestId;
  status = ack.status;
  return true;
}

} // namespace soarm
