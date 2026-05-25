#include "scoped_bus_lock.h"

namespace soarm {

ScopedBusLock::ScopedBusLock(LockManager &lockManager, uint32_t timeoutMs)
    : lockManager_(lockManager), locked_(lockManager_.lock(LockDomain::State, timeoutMs)) {
}

ScopedBusLock::~ScopedBusLock() {
  if (locked_) {
    lockManager_.unlock(LockDomain::State);
  }
}

bool ScopedBusLock::locked() const {
  return locked_;
}

} // namespace soarm
