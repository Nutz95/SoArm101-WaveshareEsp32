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
  virtual void updateLastCommandAck(uint16_t requestId, uint8_t op, uint8_t status) = 0;
  virtual void requestImmediatePresenceTx() = 0;
  virtual void updateServoTelemetry(
      const char *servoIds,
      const char *servoTelemetry,
      uint8_t servoCount,
      bool debugManual) = 0;
};

} // namespace soarm
