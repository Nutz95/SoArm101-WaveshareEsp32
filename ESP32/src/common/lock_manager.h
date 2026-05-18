#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace soarm {

enum class LockDomain : uint8_t {
  Telemetry = 0,
  Command = 1,
  State = 2
};

class LockManager {
public:
  LockManager() {
    telemetryMutex_ = xSemaphoreCreateMutex();
    commandMutex_ = xSemaphoreCreateMutex();
    stateMutex_ = xSemaphoreCreateMutex();
  }

  ~LockManager() {
    if (telemetryMutex_ != nullptr) {
      vSemaphoreDelete(telemetryMutex_);
    }
    if (commandMutex_ != nullptr) {
      vSemaphoreDelete(commandMutex_);
    }
    if (stateMutex_ != nullptr) {
      vSemaphoreDelete(stateMutex_);
    }
  }

  bool lock(LockDomain domain, TickType_t timeoutTicks = pdMS_TO_TICKS(20)) {
    SemaphoreHandle_t target = mutexFor(domain);
    if (target == nullptr) {
      return false;
    }
    return xSemaphoreTake(target, timeoutTicks) == pdTRUE;
  }

  void unlock(LockDomain domain) {
    SemaphoreHandle_t target = mutexFor(domain);
    if (target != nullptr) {
      xSemaphoreGive(target);
    }
  }

private:
  SemaphoreHandle_t telemetryMutex_{nullptr};
  SemaphoreHandle_t commandMutex_{nullptr};
  SemaphoreHandle_t stateMutex_{nullptr};

  SemaphoreHandle_t mutexFor(LockDomain domain) {
    switch (domain) {
    case LockDomain::Telemetry:
      return telemetryMutex_;
    case LockDomain::Command:
      return commandMutex_;
    case LockDomain::State:
      return stateMutex_;
    default:
      return nullptr;
    }
  }
};

} // namespace soarm
