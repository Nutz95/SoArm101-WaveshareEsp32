#pragma once

#include <cstdint>

namespace soarm {

enum class CommandAckStatus : uint8_t {
  None = 0,
  Accepted = 1,
  Applied = 2,
  Failed = 3,
  Timeout = 4,
  Rejected = 5,
};

} // namespace soarm
