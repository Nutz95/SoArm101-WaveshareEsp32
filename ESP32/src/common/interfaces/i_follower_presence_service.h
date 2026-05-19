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
  virtual bool consumeServoScanRequested() = 0;
  virtual bool consumeServoControl(uint8_t &op, uint32_t &value) = 0;
  virtual void updateServoTelemetry(
      const char *servoIds,
      const char *servoTelemetry,
      uint8_t servoCount,
      bool debugManual) = 0;
};

} // namespace soarm
