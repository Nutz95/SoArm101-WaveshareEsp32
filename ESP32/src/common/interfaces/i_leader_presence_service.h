#pragma once

#include <cstdint>

namespace soarm {

class ILeaderPresenceService {
public:
  virtual ~ILeaderPresenceService() = default;

  virtual bool begin() = 0;
  virtual void tick() = 0;
  virtual bool isFollowerLinked() const = 0;
  virtual bool hasValidFollowerIp() const = 0;
  virtual const char *followerIp() const = 0;
  virtual bool isPaired() const = 0;
  virtual const char *pairedPeerMac() const = 0;
  virtual const char *localMac() const = 0;
  virtual bool resetPairing() = 0;
  virtual bool requestServoScan(uint16_t requestId) = 0;
  virtual bool requestServoControl(uint8_t op, uint32_t value, uint16_t requestId) = 0;
  virtual bool requestTeleopMirrorBatch(
      const uint8_t *ids,
      const int16_t *positions,
      uint8_t count,
      uint8_t speedPct,
      uint16_t requestId) = 0;
  virtual const char *followerServoIds() const = 0;
  virtual const char *followerServoTelemetry() const = 0;
  virtual uint8_t followerServoCount() const = 0;
  virtual bool followerServoDebugManual() const = 0;
  virtual bool followerServoTemperatureAlarm() const = 0;
  virtual uint16_t followerLastAckRequestId() const = 0;
  virtual uint8_t followerLastAckCommandOp() const = 0;
  virtual uint8_t followerLastAckStatus() const = 0;
  virtual uint32_t followerLastAckMs() const = 0;
};

} // namespace soarm
