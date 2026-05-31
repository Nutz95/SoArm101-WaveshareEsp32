#pragma once

#include <Arduino.h>
#include <HardwareSerial.h>
#include <cstdint>

namespace soarm {

// Reads teleop batch frames from USB CDC Serial (CP2102), fed by a PC COM bridge from the leader.
class FollowerTeleopPcSerialBridge {
public:
  void attach(HardwareSerial &serial);
  bool consumeBatch(
      uint8_t *ids,
      int16_t *positions,
      uint8_t capacity,
      uint8_t &count,
      uint8_t &speedPercent,
      uint16_t &requestId,
      uint8_t &flags);
  // Read all pending serial frames and return only the newest batch.
  bool drainLatestBatch(
      uint8_t *ids,
      int16_t *positions,
      uint8_t capacity,
      uint8_t &count,
      uint8_t &speedPercent,
      uint16_t &requestId,
      uint8_t &flags);

private:
  HardwareSerial *serial_{nullptr};
};

} // namespace soarm
