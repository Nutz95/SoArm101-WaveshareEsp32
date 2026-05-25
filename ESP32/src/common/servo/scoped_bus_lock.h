#pragma once

#include "../lock_manager.h"

#include <cstdint>

namespace soarm {

class ScopedBusLock {
public:
  explicit ScopedBusLock(LockManager &lockManager, uint32_t timeoutMs);
  ~ScopedBusLock();

  ScopedBusLock(const ScopedBusLock &) = delete;
  ScopedBusLock &operator=(const ScopedBusLock &) = delete;

  bool locked() const;

private:
  LockManager &lockManager_;
  bool locked_;
};

} // namespace soarm