#include "wifi_direct_session.h"

#include "../presence/presence_message_type.h"

#include <cstdio>
#include <cstring>

#if defined(UNIT_TEST_HOST)
#include <random>
#else
#include <esp_random.h>
#endif

namespace soarm {

namespace {

constexpr char kHexAlphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";

void fillRandomToken(char *buffer, size_t bufferSize, size_t tokenLength) {
  if (buffer == nullptr || bufferSize == 0U || tokenLength == 0U) {
    return;
  }

  const size_t safeLength = tokenLength < (bufferSize - 1U) ? tokenLength : (bufferSize - 1U);
  for (size_t i = 0U; i < safeLength; ++i) {
#if defined(UNIT_TEST_HOST)
    static std::mt19937 rng(0x534F4152U);
    const uint32_t value = rng();
#else
    const uint32_t value = esp_random();
#endif
    buffer[i] = kHexAlphabet[value % (sizeof(kHexAlphabet) - 1U)];
  }
  buffer[safeLength] = '\0';
}

bool copyField(char *dst, size_t dstSize, const char *src) {
  if (dst == nullptr || dstSize == 0U || src == nullptr) {
    return false;
  }
  strncpy(dst, src, dstSize - 1U);
  dst[dstSize - 1U] = '\0';
  return dst[0] != '\0';
}

} // namespace

bool generateWifiDirectCredentials(uint32_t sessionId, WifiDirectCredentials &out) {
  out.sessionId = sessionId;
  out.channel = 1U;

  char suffix[8]{};
  fillRandomToken(suffix, sizeof(suffix), 4U);
  snprintf(out.ssid, sizeof(out.ssid), "soarm-%s", suffix);

  fillRandomToken(out.psk, sizeof(out.psk), 12U);
  if (out.psk[0] == '\0') {
    return false;
  }
  return true;
}

void buildWifiDirectOfferPacket(const WifiDirectCredentials &credentials, WifiDirectOfferPacket &out) {
  memset(&out, 0, sizeof(out));
  out.magic = kPresenceMagic;
  out.version = kWifiDirectPacketVersion;
  out.messageType = static_cast<uint8_t>(PresenceMessageType::WifiDirectOffer);
  out.channel = credentials.channel;
  out.sessionId = credentials.sessionId;
  out.teleopPort = kWifiDirectTeleopPort;
  copyField(out.ssid, sizeof(out.ssid), credentials.ssid);
  copyField(out.psk, sizeof(out.psk), credentials.psk);
  copyField(out.leaderApIp, sizeof(out.leaderApIp), kWifiDirectLeaderApIp);
}

bool validateWifiDirectOfferPacket(const WifiDirectOfferPacket &packet, WifiDirectCredentials &out) {
  if (packet.magic != kPresenceMagic || packet.version != kWifiDirectPacketVersion) {
    return false;
  }
  if (packet.messageType != static_cast<uint8_t>(PresenceMessageType::WifiDirectOffer)) {
    return false;
  }
  if (packet.ssid[0] == '\0' || packet.psk[0] == '\0') {
    return false;
  }

  out.sessionId = packet.sessionId;
  out.channel = packet.channel == 0U ? 1U : packet.channel;
  copyField(out.ssid, sizeof(out.ssid), packet.ssid);
  copyField(out.psk, sizeof(out.psk), packet.psk);
  return true;
}

void buildWifiDirectAckPacket(
    uint32_t sessionId,
    WifiDirectAckStatus status,
    const char *followerStaIp,
    WifiDirectAckPacket &out) {
  memset(&out, 0, sizeof(out));
  out.magic = kPresenceMagic;
  out.version = kWifiDirectPacketVersion;
  out.messageType = static_cast<uint8_t>(PresenceMessageType::WifiDirectAck);
  out.status = static_cast<uint8_t>(status);
  out.sessionId = sessionId;
  if (followerStaIp != nullptr) {
    copyField(out.followerStaIp, sizeof(out.followerStaIp), followerStaIp);
  }
}

bool validateWifiDirectAckPacket(const WifiDirectAckPacket &packet, uint32_t expectedSessionId) {
  if (packet.magic != kPresenceMagic || packet.version != kWifiDirectPacketVersion) {
    return false;
  }
  if (packet.messageType != static_cast<uint8_t>(PresenceMessageType::WifiDirectAck)) {
    return false;
  }
  if (packet.sessionId != expectedSessionId) {
    return false;
  }
  return true;
}

} // namespace soarm
