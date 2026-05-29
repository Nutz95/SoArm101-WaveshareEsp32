#include "servo_bus_service.h"
#include "scoped_bus_lock.h"

#include "../../Config/common_runtime_config.h"

#include <Arduino.h>
#include <SCServo.h>
#include <cstdio>

namespace soarm {

namespace {

constexpr int kCenterPositionRaw = 2048;
constexpr int kCenterVerifyToleranceRaw = 256;
constexpr uint8_t kCenterVerifyAttempts = 10U;

uint8_t parseDetectedIds(const char *idsText, uint8_t *ids, uint8_t maxCount) {
  if (idsText == nullptr || ids == nullptr || maxCount == 0U) {
    return 0U;
  }
  uint8_t count = 0U;
  for (const char *cursor = idsText; *cursor != '\0' && count < maxCount;) {
    unsigned int id = 0U;
    if (sscanf(cursor, "%u", &id) == 1 && id <= 255U) {
      ids[count++] = static_cast<uint8_t>(id);
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

bool ServoBusService::calibrateOffsetsForDetectedServos() {
  if (!started_ || serial_ == nullptr) {
    setSummary("not started");
    return false;
  }

  uint8_t ids[16]{};
  uint8_t count = parseDetectedIds(lastIdsText_, ids, static_cast<uint8_t>(sizeof(ids) / sizeof(ids[0])));
  if (count == 0U) {
    scan();
    count = parseDetectedIds(lastIdsText_, ids, static_cast<uint8_t>(sizeof(ids) / sizeof(ids[0])));
  }
  if (count == 0U) {
    setSummary("no servo found");
    return false;
  }

  ScopedBusLock guard(lockManager_, config::common::kServoBusLockTimeoutMs);
  if (!guard.locked()) {
    setSummary("bus busy");
    return false;
  }

  SMS_STS driver;
  driver.End = 0;
  driver.pSerial = serial_;
  driver.IOTimeOut = config::common::kServoBusIoTimeoutMs;
  for (uint8_t i = 0U; i < count; ++i) {
    const uint8_t id = ids[i];
    if (driver.Ping(id) < 0) {
      setSummary("cal center ping fail");
      return false;
    }
    if (driver.CalibrationOfs(id) <= 0) {
      setSummary("cal center failed");
      return false;
    }
    bool centered = false;
    for (uint8_t attempt = 0U; attempt < kCenterVerifyAttempts; ++attempt) {
      delay(20);
      const int position = driver.ReadPos(id);
      if (position >= (kCenterPositionRaw - kCenterVerifyToleranceRaw) &&
          position <= (kCenterPositionRaw + kCenterVerifyToleranceRaw)) {
        centered = true;
        break;
      }
    }
    if (!centered) {
      setSummary("cal center verify fail");
      return false;
    }
  }

  setSummary("cal center ready");
  return true;
}

} // namespace soarm