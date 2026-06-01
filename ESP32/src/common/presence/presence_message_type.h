#pragma once

#include <cstdint>

namespace soarm {

enum class PresenceMessageType : uint8_t {
  PairRequest = 1,
  PairAck = 2,
  Presence = 3,
  PairReset = 4,
  ServoScan = 5,
  ServoControl = 6,
  ServoCommandAck = 7,
  ServoControlBatch = 8,
  LinkHeartbeat = 9,
  WifiDirectOffer = 10,
  WifiDirectAck = 11,
};

} // namespace soarm
