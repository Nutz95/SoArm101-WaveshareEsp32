#pragma once

#include "../lock_manager.h"
#include "servo_position_snapshot.h"

#include <cstdint>

class HardwareSerial;

namespace soarm {

struct ServoBusConfig {
  HardwareSerial *serial{nullptr};
  int rxPin{-1};
  int txPin{-1};
  uint32_t baudRate{1000000U};
  uint8_t firstId{1U};
  uint8_t lastId{32U};
};

class ServoBusService {
public:
  ServoBusService();

  bool begin(const ServoBusConfig &config);
  uint8_t scan();
  uint8_t refreshKnownTelemetryFast();
  uint8_t refreshKnownTelemetrySync();
  /// Batched sync read of STS present load for known servo IDs (slots 1..maxSlots by ID).
  /// When non-null, gripperPresentPosOut receives J6 present position from the same block.
  uint8_t syncReadPresentLoad(int16_t *loadsOut, uint8_t maxSlots, int16_t *gripperPresentPosOut);
  bool copyPositionSnapshot(ServoPositionSnapshot &out) const;
  bool moveBatch(const uint8_t *ids, const int16_t *positions, uint8_t count, uint16_t speed);
  bool moveBatch(
      const uint8_t *ids,
      const int16_t *positions,
      uint8_t count,
      uint16_t speed,
      bool enableTorqueBeforeWrite);
  bool moveTo(uint8_t id, int16_t position, uint16_t speed, uint8_t acceleration);
  bool setServoId(uint8_t oldId, uint8_t newId);
  bool ping(uint8_t id);
  bool setServoMode(uint8_t id, uint8_t mode);
  bool setTorqueEnabled(uint8_t id, bool enabled);
  bool setTorqueEnabledForDetectedServos(bool enabled);
  /// Per-joint compliance hold for teleop haptic overlay (torque limit + goal = present).
  bool applyTeleopHapticFrame(
      const uint8_t *ids,
      const int16_t *positions,
      const uint16_t *torqueLimits,
      const bool *releaseTorque,
      uint8_t count);
  bool calibrateOffsetsForDetectedServos();
  void setDebugManual(bool enabled);
  bool isDebugManual() const;
  bool pollTemperatureAlarmSlow();
  bool hasTemperatureAlarm() const;
  int16_t lastMaxTemperatureC() const;
  const char *lastScanSummary() const;
  uint8_t lastScanCount() const;
  const char *lastIdsText() const;
  const char *lastTelemetryText() const;

private:
  void setSummary(const char *text);
  void updatePositionSnapshot(const ServoPositionSnapshot &snapshot);

  ServoBusConfig config_{};
  HardwareSerial *serial_{nullptr};
  bool started_{false};
  bool debugManual_{false};
  uint8_t lastScanCount_{0U};
  bool temperatureAlarm_{false};
  int16_t lastMaxTemperatureC_{0};
  uint32_t lastTemperaturePollMs_{0U};
  char lastScanSummary_[64]{};
  char lastIdsText_[48]{};
  char lastTelemetryText_[96]{};
  ServoPositionSnapshot positionSnapshot_{};
  LockManager lockManager_{};
};

} // namespace soarm