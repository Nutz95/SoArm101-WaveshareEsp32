#pragma once

#include <Arduino.h>
#include <HardwareSerial.h>
#include <cstdint>

namespace soarm {

// Sends teleop batch frames on USB CDC Serial (CP2102). A PC COM bridge forwards to the follower.
class LeaderTeleopPcSerialBridge {
public:
  void attach(HardwareSerial &serial);
  bool sendBatch(
      const uint8_t *ids,
      const int16_t *positions,
      uint8_t count,
      uint8_t speedPercent,
      uint16_t requestId,
      uint8_t flags);

private:
  HardwareSerial *serial_{nullptr};
};

} // namespace soarm
