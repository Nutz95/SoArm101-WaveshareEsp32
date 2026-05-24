#include "servo_bus_service.h"

#include <Arduino.h>
#include <SCServo.h>
#include <cstring>

namespace soarm {

namespace {
constexpr uint8_t kServoModePosition = 0U;
constexpr uint8_t kServoModePwm = 1U;
constexpr uint16_t kPositionMinRaw = 0U;
constexpr uint16_t kPositionMaxRaw = 4095U;
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

  SCSCL driver;
  driver.End = 0;
  driver.pSerial = serial_;
  driver.IOTimeOut = 20U;

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

bool ServoBusService::moveTo(uint8_t id, int16_t position, uint16_t speed, uint8_t acceleration) {
  if (!started_ || serial_ == nullptr) {
    setSummary("not started");
    return false;
  }

  SCSCL driver;
  driver.End = 0;
  driver.pSerial = serial_;
  driver.IOTimeOut = 20U;

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

  SCSCL driver;
  driver.End = 0;
  driver.pSerial = serial_;
  driver.IOTimeOut = 20U;

  if (driver.unLockEprom(oldId) <= 0) {
    setSummary("unlock failed");
    return false;
  }
  if (driver.writeByte(oldId, SCSCL_ID, newId) <= 0) {
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
  for (uint8_t attempt = 0U; attempt < 3U; ++attempt) {
    if (driver.Ping(newId) >= 0) {
      newIdResponds = true;
      break;
    }
    delay(4);
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

  SCSCL driver;
  driver.End = 0;
  driver.pSerial = serial_;
  driver.IOTimeOut = 20U;
  return driver.Ping(id) >= 0;
}

bool ServoBusService::setServoMode(uint8_t id, uint8_t mode) {
  if (!started_ || serial_ == nullptr) {
    setSummary("not started");
    return false;
  }

  SCSCL driver;
  driver.End = 0;
  driver.pSerial = serial_;
  driver.IOTimeOut = 20U;

  int result = 0;
  if (mode == kServoModePwm) {
    if (driver.unLockEprom(id) <= 0) {
      setSummary("unlock failed");
      return false;
    }
    result = driver.PWMMode(id);
    if (driver.LockEprom(id) <= 0) {
      setSummary("lock failed");
      return false;
    }
  } else if (mode == kServoModePosition) {
    if (driver.unLockEprom(id) <= 0) {
      setSummary("unlock failed");
      return false;
    }
    if (driver.writeWord(id, SCSCL_MIN_ANGLE_LIMIT_L, kPositionMinRaw) <= 0) {
      setSummary("min limit failed");
      return false;
    }
    if (driver.writeWord(id, SCSCL_MAX_ANGLE_LIMIT_L, kPositionMaxRaw) <= 0) {
      setSummary("max limit failed");
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