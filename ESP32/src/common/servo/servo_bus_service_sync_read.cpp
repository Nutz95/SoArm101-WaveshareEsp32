#include "servo_bus_service.h"
#include "scoped_bus_lock.h"

#include "../../Config/common_runtime_config.h"

#include <SCServo.h>
#include <cstdio>
#include <cstring>

namespace soarm {

namespace {

uint8_t parseKnownIds(const char *idsText, uint8_t *ids, uint8_t maxCount) {
  if (idsText == nullptr || ids == nullptr || maxCount == 0U) {
    return 0U;
  }

  uint8_t count = 0U;
  const char *cursor = idsText;
  while (*cursor != '\0' && count < maxCount) {
    unsigned int id = 0U;
    if (sscanf(cursor, "%u", &id) == 1 && id <= 255U) {
      ids[count] = static_cast<uint8_t>(id & 0xFFU);
      ++count;
    }

    while (*cursor != '\0' && *cursor != ',') {
      ++cursor;
    }
    if (*cursor == ',') {
      ++cursor;
    }
  }

  return count;
}

void appendTelemetryFragment(char *telemetryText, size_t telemetryCapacity, uint8_t id, int position) {
  char telemetryFragment[20];
  snprintf(telemetryFragment, sizeof(telemetryFragment), "#%u p%d;", id, position);
  strncat(telemetryText, telemetryFragment, telemetryCapacity - strlen(telemetryText) - 1U);
}

void flushSerialRx(HardwareSerial *serial) {
  if (serial == nullptr) {
    return;
  }
  while (serial->available() > 0) {
    serial->read();
  }
}

uint8_t readPositionsSync(
    SMS_STS &driver,
    const uint8_t *ids,
    uint8_t knownCount,
    char *telemetryText,
    size_t telemetryCapacity,
    ServoPositionSnapshot &snapshot) {
  uint8_t availableCount = 0U;
  for (uint8_t i = 0U; i < knownCount; ++i) {
    const uint8_t id = ids[i];
    uint8_t rxBuffer[4]{};
    if (driver.syncReadPacketRx(id, rxBuffer) != 2) {
      continue;
    }

    const int position = driver.syncReadRxPacketToWrod(15);
    if (position < 0) {
      continue;
    }

    appendTelemetryFragment(telemetryText, telemetryCapacity, id, position);
    if (availableCount < config::common::kTeleopBatchMaxServos) {
      snapshot.samples[availableCount].id = id;
      snapshot.samples[availableCount].position = static_cast<int16_t>(position);
      ++availableCount;
    }
  }
  return availableCount;
}

} // namespace

uint8_t ServoBusService::refreshKnownTelemetrySync() {
  if (!started_ || serial_ == nullptr) {
    setSummary("not started");
    return 0U;
  }

  uint8_t ids[16]{};
  const uint8_t knownCount = parseKnownIds(lastIdsText_, ids, static_cast<uint8_t>(sizeof(ids) / sizeof(ids[0])));
  if (knownCount == 0U) {
    return scan();
  }

  ScopedBusLock guard(lockManager_, config::common::kServoBusLockTimeoutMs);
  if (!guard.locked()) {
    setSummary("bus busy");
    return lastScanCount_;
  }

  SMS_STS driver;
  driver.End = 0;
  driver.pSerial = serial_;
  driver.IOTimeOut = config::common::kServoBusIoTimeoutMs;
  driver.Level = 0;

  flushSerialRx(serial_);

  if (driver.syncReadPacketTx(ids, knownCount, SMS_STS_PRESENT_POSITION_L, 2) != 2) {
    setSummary("sync read tx fail");
    return lastScanCount_;
  }

  serial_->flush();

  char telemetryText[96];
  telemetryText[0] = '\0';
  ServoPositionSnapshot snapshot{};
  snapshot.capturedAtMs = millis();

  const uint8_t availableCount =
      readPositionsSync(driver, ids, knownCount, telemetryText, sizeof(telemetryText), snapshot);

  snapshot.count = availableCount;
  updatePositionSnapshot(snapshot);

  if (availableCount == 0U) {
    strncpy(lastTelemetryText_, "-", sizeof(lastTelemetryText_) - 1U);
    lastTelemetryText_[sizeof(lastTelemetryText_) - 1U] = '\0';
    lastScanCount_ = 0U;
    setSummary("sync read empty");
    return 0U;
  }

  strncpy(lastTelemetryText_, telemetryText, sizeof(lastTelemetryText_) - 1U);
  lastTelemetryText_[sizeof(lastTelemetryText_) - 1U] = '\0';
  lastScanCount_ = availableCount;
  setSummary("sync read ok");
  return availableCount;
}

} // namespace soarm
