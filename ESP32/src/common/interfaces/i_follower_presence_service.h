#pragma once

#include <cstdint>

namespace soarm {

class IFollowerPresenceService {
public:
  virtual ~IFollowerPresenceService() = default;

  virtual bool begin() = 0;
  virtual void tick(const char *localIp) = 0;
  virtual bool isPaired() const = 0;
  virtual const char *pairedPeerMac() const = 0;
  virtual const char *localMac() const = 0;
  virtual bool resetPairing() = 0;
  virtual bool consumeServoScanRequested(uint16_t &requestId) = 0;
  virtual bool consumeServoControl(uint8_t &op, uint32_t &value, uint16_t &requestId) = 0;
  virtual bool consumeTeleopMirrorBatch(
      uint8_t *ids,
      int16_t *positions,
      uint8_t capacity,
      uint8_t &count,
      uint8_t &speedPct,
      uint16_t &requestId) = 0;
  virtual void updateLastCommandAck(uint16_t requestId, uint8_t op, uint8_t status) = 0;
  // Teleop batches: update ACK fields for the next periodic presence only (no ESP-NOW ACK flood).
  virtual void stageTeleopBatchAck(uint16_t requestId, uint8_t status) = 0;
  virtual void requestImmediatePresenceTx() = 0;
  virtual void sendLinkKeepalive(const char *localIp) = 0;
  virtual void notifyWifiTeleopActivity() = 0;
  virtual void updateServoTelemetry(
      const char *servoIds,
      const char *servoTelemetry,
      uint8_t servoCount,
      bool debugManual,
      bool temperatureAlarm) = 0;
};

} // namespace soarm
