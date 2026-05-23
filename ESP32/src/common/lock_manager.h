#pragma once

#include <cstdint>
#include <memory>

namespace soarm {

enum class LockDomain : uint8_t {
  Telemetry = 0,
  Command = 1,
  State = 2
};

class LockManager {
public:
  LockManager();
  ~LockManager();

  LockManager(const LockManager &) = delete;
  LockManager &operator=(const LockManager &) = delete;
  LockManager(LockManager &&) noexcept;
  LockManager &operator=(LockManager &&) noexcept;

  bool lock(LockDomain domain, uint32_t timeoutMs = 20U);
  void unlock(LockDomain domain);

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace soarm
