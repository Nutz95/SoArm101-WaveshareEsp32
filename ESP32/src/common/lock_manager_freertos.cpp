#include "lock_manager.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace soarm {

struct LockManager::Impl {
  SemaphoreHandle_t telemetryMutex{nullptr};
  SemaphoreHandle_t commandMutex{nullptr};
  SemaphoreHandle_t stateMutex{nullptr};

  Impl() {
    telemetryMutex = xSemaphoreCreateMutex();
    commandMutex = xSemaphoreCreateMutex();
    stateMutex = xSemaphoreCreateMutex();
  }

  ~Impl() {
    if (telemetryMutex != nullptr) {
      vSemaphoreDelete(telemetryMutex);
    }
    if (commandMutex != nullptr) {
      vSemaphoreDelete(commandMutex);
    }
    if (stateMutex != nullptr) {
      vSemaphoreDelete(stateMutex);
    }
  }

  bool lock(LockDomain domain, uint32_t timeoutMs) {
    SemaphoreHandle_t target = mutexFor(domain);
    if (target == nullptr) {
      return false;
    }
    return xSemaphoreTake(target, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
  }

  void unlock(LockDomain domain) {
    SemaphoreHandle_t target = mutexFor(domain);
    if (target != nullptr) {
      xSemaphoreGive(target);
    }
  }

  SemaphoreHandle_t mutexFor(LockDomain domain) {
    switch (domain) {
    case LockDomain::Telemetry:
      return telemetryMutex;
    case LockDomain::Command:
      return commandMutex;
    case LockDomain::State:
      return stateMutex;
    default:
      return nullptr;
    }
  }
};

LockManager::LockManager()
    : impl_(new Impl()) {
}

LockManager::~LockManager() = default;

LockManager::LockManager(LockManager &&) noexcept = default;

LockManager &LockManager::operator=(LockManager &&) noexcept = default;

bool LockManager::lock(LockDomain domain, uint32_t timeoutMs) {
  return impl_ != nullptr && impl_->lock(domain, timeoutMs);
}

void LockManager::unlock(LockDomain domain) {
  if (impl_ != nullptr) {
    impl_->unlock(domain);
  }
}

} // namespace soarm