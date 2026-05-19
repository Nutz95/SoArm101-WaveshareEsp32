#pragma once

#include <cstdint>

namespace soarm {

enum class PresenceMessageType : uint8_t {
  PairRequest = 1,
  PairAck = 2,
  Presence = 3
};

} // namespace soarm
