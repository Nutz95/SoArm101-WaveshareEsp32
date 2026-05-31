#include "leader_teleop_wifi_bridge.h"

#include "../Config/common_runtime_config.h"
#include "../Config/leader_runtime_config.h"
#include "../common/teleop/teleop_follower_endpoint.h"
#include "../common/teleop/teleop_packet_flags.h"

#include <Arduino.h>
#include <WiFi.h>
#include <cerrno>
#include <cstring>

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
    uint16_t requestId,
    uint8_t flags) {
  if (!started_ || ids == nullptr || positions == nullptr || count == 0U) {
    return false;
  }

  const uint32_t nowMs = millis();
  if (nowMs < sendBackoffUntilMs_) {
    return false;
  }

  char resolvedIp[16]{};
  if (hasCachedEndpoint_ && followerIp != nullptr && strcmp(followerIp, cachedSourceIp_) == 0) {
    strncpy(resolvedIp, cachedEndpoint_, sizeof(resolvedIp) - 1U);
    resolvedIp[sizeof(resolvedIp) - 1U] = '\0';
  } else if (!resolveFollowerEndpoint(followerIp, resolvedIp, sizeof(resolvedIp))) {
    hasCachedEndpoint_ = false;
    return false;
  } else {
    strncpy(cachedEndpoint_, resolvedIp, sizeof(cachedEndpoint_) - 1U);
    cachedEndpoint_[sizeof(cachedEndpoint_) - 1U] = '\0';
    if (followerIp != nullptr) {
      strncpy(cachedSourceIp_, followerIp, sizeof(cachedSourceIp_) - 1U);
      cachedSourceIp_[sizeof(cachedSourceIp_) - 1U] = '\0';
    } else {
      cachedSourceIp_[0] = '\0';
    }
    hasCachedEndpoint_ = true;
  }

  IPAddress remoteIp;
  if (!remoteIp.fromString(resolvedIp)) {
    return false;
  }

  const uint8_t clampedCount = (count > config::common::kTeleopBatchMaxServos)
                                  ? config::common::kTeleopBatchMaxServos
                                  : count;
  teleop_wifi::BatchPacket packet{};
  packet.magic = teleop_wifi::kMagic;
  packet.version = teleop_wifi::kVersion;
  packet.flags = flags;
  packet.type = teleop_wifi::kTypeBatch;
  packet.requestId = requestId;
  packet.count = clampedCount;
  packet.speedPercent = speedPercent;
  for (uint8_t i = 0U; i < clampedCount; ++i) {
    packet.entries[i].id = ids[i];
    packet.entries[i].position = positions[i];
  }

  if (udp_.beginPacket(remoteIp, teleop_wifi::kFollowerListenPort) != 1) {
    sendBackoffUntilMs_ = nowMs + config::leader::kTeleopWifiSendBackoffMs;
    return false;
  }

  const size_t written = udp_.write(reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
  if (written != sizeof(packet)) {
    udp_.endPacket();
    sendBackoffUntilMs_ = nowMs + config::leader::kTeleopWifiSendBackoffMs;
    logSendErrorThrottled(nowMs, errno, requestId, clampedCount, resolvedIp, "write");
    return false;
  }

  const bool sent = udp_.endPacket() == 1;
  if (!sent) {
    sendBackoffUntilMs_ = nowMs + config::leader::kTeleopWifiSendBackoffMs;
    logSendErrorThrottled(nowMs, errno, requestId, clampedCount, resolvedIp, "endPacket");
    return false;
  }

  sendBackoffUntilMs_ = 0U;
  return true;
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

void LeaderTeleopWifiBridge::logSendErrorThrottled(
    uint32_t nowMs,
    int errorCode,
    uint16_t requestId,
    uint8_t count,
    const char *ip,
    const char *stage) {
  if ((nowMs - lastSendErrorLogMs_) < config::leader::kTeleopWifiSendErrorLogIntervalMs) {
    return;
  }
  lastSendErrorLogMs_ = nowMs;
  Serial.printf("[TELEOP][WIFI] %s fail errno=%d id=%u count=%u ip=%s\n",
                stage,
                errorCode,
                requestId,
                count,
                ip);
}

} // namespace soarm
