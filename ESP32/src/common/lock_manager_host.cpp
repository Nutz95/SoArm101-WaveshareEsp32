#include "lock_manager.h"

namespace soarm {

struct LockManager::Impl {
  bool lock(LockDomain) {
    return true;
  }

  void unlock(LockDomain) {
  }
};

LockManager::LockManager()
    : impl_(new Impl()) {
}

LockManager::~LockManager() = default;

LockManager::LockManager(LockManager &&) noexcept = default;

LockManager &LockManager::operator=(LockManager &&) noexcept = default;

bool LockManager::lock(LockDomain domain, uint32_t timeoutMs) {
  (void)timeoutMs;
  return impl_ != nullptr && impl_->lock(domain);
}

void LockManager::unlock(LockDomain domain) {
  if (impl_ != nullptr) {
    impl_->unlock(domain);
  }
}

} // namespace soarm