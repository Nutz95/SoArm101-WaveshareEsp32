#include "servo_bus_service.h"
#include "scoped_bus_lock.h"

#include "../../Config/common_runtime_config.h"

#include <Arduino.h>
#include <SCServo.h>
#include <cstdio>

namespace soarm {

namespace {

uint8_t parseKnownIdsForTemperature(const char *idsText, uint8_t *ids, uint8_t maxCount) {
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

bool ServoBusService::pollTemperatureAlarmSlow() {
  if (!started_ || serial_ == nullptr) {
    return temperatureAlarm_;
  }

  const uint32_t nowMs = millis();
  if ((nowMs - lastTemperaturePollMs_) < config::common::kServoTemperaturePollIntervalMs) {
    return temperatureAlarm_;
  }
  lastTemperaturePollMs_ = nowMs;

  uint8_t ids[16]{};
  const uint8_t knownCount = parseKnownIdsForTemperature(
      lastIdsText_,
      ids,
      static_cast<uint8_t>(sizeof(ids) / sizeof(ids[0])));
  if (knownCount == 0U) {
    return temperatureAlarm_;
  }

  ScopedBusLock guard(lockManager_, config::common::kServoBusLockTimeoutMs);
  if (!guard.locked()) {
    return temperatureAlarm_;
  }

  SMS_STS driver;
  driver.End = 0;
  driver.pSerial = serial_;
  driver.IOTimeOut = config::common::kServoBusIoTimeoutMs;

  bool hasTemperatureSample = false;
  int16_t maxTemperature = -32768;
  for (uint8_t i = 0U; i < knownCount; ++i) {
    const uint8_t id = ids[i];
    if (driver.Ping(id) < 0) {
      continue;
    }

    const int rawTemperature = driver.ReadTemper(id);
    if (rawTemperature < 0) {
      continue;
    }

    const int16_t temperature = static_cast<int16_t>(rawTemperature);
    if (!hasTemperatureSample || temperature > maxTemperature) {
      maxTemperature = temperature;
    }
    hasTemperatureSample = true;
  }

  if (!hasTemperatureSample) {
    return temperatureAlarm_;
  }

  lastMaxTemperatureC_ = maxTemperature;

  if (!temperatureAlarm_ && maxTemperature >= config::common::kServoTemperatureAlarmThresholdC) {
    temperatureAlarm_ = true;
  } else if (temperatureAlarm_ && maxTemperature <= config::common::kServoTemperatureAlarmClearThresholdC) {
    temperatureAlarm_ = false;
  }

  return temperatureAlarm_;
}

bool ServoBusService::hasTemperatureAlarm() const {
  return temperatureAlarm_;
}

int16_t ServoBusService::lastMaxTemperatureC() const {
  return lastMaxTemperatureC_;
}

} // namespace soarm
