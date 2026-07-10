#include "servo_bus_service.h"
#include "scoped_bus_lock.h"

#include "../../Config/common_runtime_config.h"

#include <SCServo.h>

namespace soarm {

bool ServoBusService::applyTeleopHapticFrame(
    const uint8_t *ids,
    const int16_t *positions,
    const uint16_t *torqueLimits,
    const bool *releaseTorque,
    uint8_t count) {
  if (!started_ || serial_ == nullptr || ids == nullptr || positions == nullptr ||
      torqueLimits == nullptr || releaseTorque == nullptr || count == 0U) {
    setSummary("haptic invalid");
    return false;
  }

  const uint8_t clampedCount = (count > config::common::kTeleopBatchMaxServos)
                                   ? config::common::kTeleopBatchMaxServos
                                   : count;

  ScopedBusLock guard(lockManager_, config::common::kServoBusLockTimeoutMs);
  if (!guard.locked()) {
    setSummary("bus busy");
    return false;
  }

  SMS_STS driver;
  driver.End = 0;
  driver.pSerial = serial_;
  driver.IOTimeOut = config::common::kServoBusIoTimeoutMs;

  bool anyOk = false;
  for (uint8_t i = 0U; i < clampedCount; ++i) {
    const uint8_t id = ids[i];
    if (id == 0U) {
      continue;
    }

    if (releaseTorque[i]) {
      if (driver.EnableTorque(id, 0U) > 0) {
        anyOk = true;
      }
      continue;
    }

    if (driver.writeWord(id, SMS_STS_TORQUE_LIMIT_L, torqueLimits[i]) <= 0) {
      continue;
    }
    if (driver.EnableTorque(id, 1U) <= 0) {
      continue;
    }
    if (driver.WritePosEx(id, positions[i], 0U, 0U) > 0) {
      anyOk = true;
    }
  }

  setSummary(anyOk ? "haptic ok" : "haptic fail");
  return anyOk;
}

} // namespace soarm
