#pragma once

namespace soarm {
namespace teleop {

// Call `consumeOnce` until it returns false; return true if at least one call succeeded.
template <typename ConsumeOnce>
inline bool drainLatestWhile(ConsumeOnce &&consumeOnce) {
  bool got = false;
  while (consumeOnce()) {
    got = true;
  }
  return got;
}

} // namespace teleop
} // namespace soarm
