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

} // namespace

uint8_t ServoBusService::refreshKnownTelemetryFast() {
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

  char telemetryText[96];
  telemetryText[0] = '\0';
  uint8_t availableCount = 0U;
  for (uint8_t i = 0U; i < knownCount; ++i) {
    const uint8_t id = ids[i];
    if (driver.Ping(id) < 0) {
      continue;
    }

    const int position = driver.ReadPos(id);
    char telemetryFragment[20];
    snprintf(telemetryFragment, sizeof(telemetryFragment), "#%u p%d;", id, position);
    strncat(telemetryText, telemetryFragment, sizeof(telemetryText) - strlen(telemetryText) - 1U);
    ++availableCount;
  }

  if (availableCount == 0U) {
    strncpy(lastTelemetryText_, "-", sizeof(lastTelemetryText_) - 1U);
    lastTelemetryText_[sizeof(lastTelemetryText_) - 1U] = '\0';
    lastScanCount_ = 0U;
    return 0U;
  }

  strncpy(lastTelemetryText_, telemetryText, sizeof(lastTelemetryText_) - 1U);
  lastTelemetryText_[sizeof(lastTelemetryText_) - 1U] = '\0';
  lastScanCount_ = availableCount;
  return availableCount;
}

} // namespace soarm
