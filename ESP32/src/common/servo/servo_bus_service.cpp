#include "servo_bus_service.h"
#include "scoped_bus_lock.h"

#include "../../Config/common_runtime_config.h"

#include <Arduino.h>
#include <SCServo.h>
#include <cstring>

namespace soarm {

namespace {
constexpr uint8_t kServoModePosition = 0U;
constexpr uint8_t kServoModePwm = 1U;
constexpr uint16_t kPositionMinRaw = 0U;
constexpr uint16_t kPositionMaxRaw = 4095U;

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
}

ServoBusService::ServoBusService() {
  setSummary("idle");
  strncpy(lastIdsText_, "-", sizeof(lastIdsText_) - 1U);
  lastIdsText_[sizeof(lastIdsText_) - 1U] = '\0';
  strncpy(lastTelemetryText_, "-", sizeof(lastTelemetryText_) - 1U);
  lastTelemetryText_[sizeof(lastTelemetryText_) - 1U] = '\0';
}

bool ServoBusService::begin(const ServoBusConfig &config) {
  config_ = config;
  serial_ = config.serial;

  if (serial_ == nullptr) {
    setSummary("serial missing");
    return false;
  }

  if (config_.rxPin >= 0 && config_.txPin >= 0) {
    serial_->begin(config_.baudRate, SERIAL_8N1, config_.rxPin, config_.txPin);
  } else {
    serial_->begin(config_.baudRate);
  }

  setSummary("ready");
  started_ = true;
  return true;
}

uint8_t ServoBusService::scan() {
  if (!started_ || serial_ == nullptr) {
    setSummary("not started");
    lastScanCount_ = 0U;
    return 0U;
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

  uint8_t foundCount = 0U;
  char summary[64];
  char idsText[48];
  char telemetryText[96];
  summary[0] = '\0';
  idsText[0] = '\0';
  telemetryText[0] = '\0';

  for (uint8_t id = config_.firstId; id <= config_.lastId; ++id) {
    const int pingResult = driver.Ping(id);
    if (pingResult >= 0) {
      ++foundCount;
      if (foundCount == 1U) {
        strncpy(summary, "found ids:", sizeof(summary) - 1);
        summary[sizeof(summary) - 1] = '\0';
      }
      char fragment[8];
      snprintf(fragment, sizeof(fragment), " %u", id);
      strncat(summary, fragment, sizeof(summary) - strlen(summary) - 1U);

      if (idsText[0] == '\0') {
        snprintf(idsText, sizeof(idsText), "%u", id);
      } else {
        snprintf(fragment, sizeof(fragment), ",%u", id);
        strncat(idsText, fragment, sizeof(idsText) - strlen(idsText) - 1U);
      }

      const int position = driver.ReadPos(id);
      const int voltage = driver.ReadVoltage(id);
      const int temperature = driver.ReadTemper(id);
      const int mode = driver.ReadMode(id);
      char telemetryFragment[40];
      snprintf(
          telemetryFragment,
          sizeof(telemetryFragment),
          "#%u p%d v%d t%d m%d ",
          id,
          position,
          voltage,
          temperature,
          mode);
      strncat(
          telemetryText,
          telemetryFragment,
          sizeof(telemetryText) - strlen(telemetryText) - 1U);
    }
  }

  lastScanCount_ = foundCount;
  if (foundCount == 0U) {
    setSummary("no servo found");
    strncpy(lastIdsText_, "-", sizeof(lastIdsText_) - 1U);
    lastIdsText_[sizeof(lastIdsText_) - 1U] = '\0';
    strncpy(lastTelemetryText_, "-", sizeof(lastTelemetryText_) - 1U);
    lastTelemetryText_[sizeof(lastTelemetryText_) - 1U] = '\0';
  } else {
    setSummary(summary);
    strncpy(lastIdsText_, idsText, sizeof(lastIdsText_) - 1U);
    lastIdsText_[sizeof(lastIdsText_) - 1U] = '\0';
    strncpy(lastTelemetryText_, telemetryText, sizeof(lastTelemetryText_) - 1U);
    lastTelemetryText_[sizeof(lastTelemetryText_) - 1U] = '\0';
  }
  return foundCount;
}

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

bool ServoBusService::moveTo(uint8_t id, int16_t position, uint16_t speed, uint8_t acceleration) {
  if (!started_ || serial_ == nullptr) {
    setSummary("not started");
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

  const int result = driver.WritePosEx(id, static_cast<int16_t>(position), speed, acceleration);
  if (result < 0) {
    setSummary("move failed");
    return false;
  }

  setSummary("move sent");
  return true;
}

bool ServoBusService::setServoId(uint8_t oldId, uint8_t newId) {
  if (!started_ || serial_ == nullptr) {
    setSummary("not started");
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

  if (driver.unLockEprom(oldId) <= 0) {
    setSummary("unlock failed");
    return false;
  }
  // Align with STS reference flow: unlock(old), write ID register, lock(new).
  if (driver.writeByte(oldId, SMS_STS_ID, newId) <= 0) {
    setSummary("set id failed");
    return false;
  }
  if (driver.LockEprom(newId) <= 0) {
    setSummary("lock failed");
    return false;
  }

  // Verify that the new ID is actually reachable before reporting success.
  // This avoids reporting a local-only success when the servo did not switch ID.
  bool newIdResponds = false;
  for (uint8_t attempt = 0U; attempt < config::common::kServoSetIdVerifyAttempts; ++attempt) {
    if (driver.Ping(newId) >= 0) {
      newIdResponds = true;
      break;
    }
    delay(config::common::kServoSetIdVerifyRetryDelayMs);
  }
  if (!newIdResponds) {
    setSummary("set id verify fail");
    return false;
  }

  if (oldId != newId && driver.Ping(oldId) >= 0) {
    setSummary("old id still active");
    return false;
  }

  setSummary("id updated");
  return true;
}

bool ServoBusService::ping(uint8_t id) {
  if (!started_ || serial_ == nullptr) {
    setSummary("not started");
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
  return driver.Ping(id) >= 0;
}

bool ServoBusService::setServoMode(uint8_t id, uint8_t mode) {
  if (!started_ || serial_ == nullptr) {
    setSummary("not started");
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

  int result = 0;
  if (mode == kServoModePwm) {
    if (driver.unLockEprom(id) <= 0) {
      setSummary("unlock failed");
      return false;
    }
    result = driver.WheelMode(id);
    if (driver.LockEprom(id) <= 0) {
      setSummary("lock failed");
      return false;
    }
  } else if (mode == kServoModePosition) {
    if (driver.unLockEprom(id) <= 0) {
      setSummary("unlock failed");
      return false;
    }
    if (driver.writeWord(id, SMS_STS_MIN_ANGLE_LIMIT_L, kPositionMinRaw) <= 0) {
      setSummary("min limit failed");
      return false;
    }
    if (driver.writeWord(id, SMS_STS_MAX_ANGLE_LIMIT_L, kPositionMaxRaw) <= 0) {
      setSummary("max limit failed");
      return false;
    }
    if (driver.writeByte(id, SMS_STS_MODE, 0U) <= 0) {
      setSummary("position mode failed");
      return false;
    }
    if (driver.LockEprom(id) <= 0) {
      setSummary("lock failed");
      return false;
    }
    result = 1;
  } else {
    setSummary("invalid mode");
    return false;
  }

  if (result <= 0) {
    setSummary("mode failed");
    return false;
  }

  setSummary("mode updated");
  return true;
}

void ServoBusService::setDebugManual(bool enabled) {
  debugManual_ = enabled;
}

bool ServoBusService::isDebugManual() const {
  return debugManual_;
}

const char *ServoBusService::lastScanSummary() const {
  return lastScanSummary_;
}

uint8_t ServoBusService::lastScanCount() const {
  return lastScanCount_;
}

const char *ServoBusService::lastIdsText() const {
  return lastIdsText_;
}

const char *ServoBusService::lastTelemetryText() const {
  return lastTelemetryText_;
}

void ServoBusService::setSummary(const char *text) {
  strncpy(lastScanSummary_, text, sizeof(lastScanSummary_) - 1U);
  lastScanSummary_[sizeof(lastScanSummary_) - 1U] = '\0';
}

} // namespace soarm