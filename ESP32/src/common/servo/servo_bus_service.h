#pragma once

#include "../lock_manager.h"

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
  bool moveBatch(const uint8_t *ids, const int16_t *positions, uint8_t count, uint16_t speed);
  bool moveTo(uint8_t id, int16_t position, uint16_t speed, uint8_t acceleration);
  bool setServoId(uint8_t oldId, uint8_t newId);
  bool ping(uint8_t id);
  bool setServoMode(uint8_t id, uint8_t mode);
  void setDebugManual(bool enabled);
  bool isDebugManual() const;
  const char *lastScanSummary() const;
  uint8_t lastScanCount() const;
  const char *lastIdsText() const;
  const char *lastTelemetryText() const;

private:
  void setSummary(const char *text);

  ServoBusConfig config_{};
  HardwareSerial *serial_{nullptr};
  bool started_{false};
  bool debugManual_{false};
  uint8_t lastScanCount_{0U};
  char lastScanSummary_[64]{};
  char lastIdsText_[48]{};
  char lastTelemetryText_[96]{};
  LockManager lockManager_{};
};

} // namespace soarm