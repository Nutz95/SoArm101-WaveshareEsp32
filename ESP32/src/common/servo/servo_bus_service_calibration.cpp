#include "servo_bus_service.h"
#include "scoped_bus_lock.h"

#include "../../Config/common_runtime_config.h"

#include <Arduino.h>
#include <SCServo.h>
#include <cstdio>
#include <cstdlib>

namespace soarm {

namespace {

constexpr int kCenterPositionRaw = 2048;
constexpr int kCenterVerifyToleranceRaw = 400;
constexpr uint8_t kCenterVerifyAttempts = 25U;
constexpr uint8_t kCenterMoveSpeed = 200U;

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

bool isNearCenter(int position) {
  return position >= (kCenterPositionRaw - kCenterVerifyToleranceRaw) &&
         position <= (kCenterPositionRaw + kCenterVerifyToleranceRaw);
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

  uint8_t offsetOkCount = 0U;
  {
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
        continue;
      }
      if (driver.CalibrationOfs(id) > 0) {
        ++offsetOkCount;
      }
    }
  }

  if (offsetOkCount == 0U) {
    setSummary("cal center offset fail");
    return false;
  }

  int16_t centerPositions[16]{};
  for (uint8_t i = 0U; i < count; ++i) {
    centerPositions[i] = static_cast<int16_t>(kCenterPositionRaw);
  }

  if (!moveBatch(ids, centerPositions, count, kCenterMoveSpeed, true)) {
    setSummary("cal center move fail");
    return false;
  }

  delay(150);

  uint8_t centeredCount = 0U;
  {
    ScopedBusLock guard(lockManager_, config::common::kServoBusLockTimeoutMs);
    if (!guard.locked()) {
      setSummary("cal center verify busy");
      return offsetOkCount > 0U;
    }

    SMS_STS driver;
    driver.End = 0;
    driver.pSerial = serial_;
    driver.IOTimeOut = config::common::kServoBusIoTimeoutMs;

    for (uint8_t attempt = 0U; attempt < kCenterVerifyAttempts; ++attempt) {
      centeredCount = 0U;
      for (uint8_t i = 0U; i < count; ++i) {
        const int position = driver.ReadPos(ids[i]);
        if (position >= 0 && isNearCenter(position)) {
          ++centeredCount;
        }
      }
      if (centeredCount >= count) {
        break;
      }
      delay(40);
    }
  }

  if (centeredCount < count) {
    snprintf(lastScanSummary_, sizeof(lastScanSummary_), "cal center %u/%u", centeredCount, count);
    lastScanSummary_[sizeof(lastScanSummary_) - 1U] = '\0';
    return centeredCount >= (count / 2U + 1U);
  }

  setSummary("cal center ready");
  return true;
}

} // namespace soarm
