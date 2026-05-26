#include "servo_bus_service.h"
#include "scoped_bus_lock.h"

#include "../../Config/common_runtime_config.h"

#include <SCServo.h>

namespace soarm {

bool ServoBusService::setTorqueEnabled(uint8_t id, bool enabled) {
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

  if (driver.EnableTorque(id, enabled ? 1U : 0U) <= 0) {
    setSummary(enabled ? "torque on failed" : "torque off failed");
    return false;
  }

  setSummary(enabled ? "torque enabled" : "torque released");
  return true;
}

bool ServoBusService::setTorqueEnabledForDetectedServos(bool enabled) {
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

  bool detectedAny = false;
  bool allOk = true;
  for (uint8_t id = config_.firstId; id <= config_.lastId; ++id) {
    if (driver.Ping(id) < 0) {
      continue;
    }

    detectedAny = true;
    if (driver.EnableTorque(id, enabled ? 1U : 0U) <= 0) {
      allOk = false;
    }
  }

  if (!detectedAny) {
    setSummary("no servo found");
    return false;
  }

  if (!allOk) {
    setSummary(enabled ? "partial torque on" : "partial torque off");
    return false;
  }

  setSummary(enabled ? "torque enabled" : "torque released");
  return true;
}

} // namespace soarm
