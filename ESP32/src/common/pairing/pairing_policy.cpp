#include "pairing_policy.h"

namespace soarm {

bool PairingPolicy::shouldAcceptLeaderPairRequest(bool hasPairedMac, bool isPairedMac) {
  if (!hasPairedMac) {
    return true;
  }
  return isPairedMac;
}

bool PairingPolicy::shouldAcceptFollowerPairAck(bool hasPairedLeaderMac, bool isSameLeaderMac) {
  if (!hasPairedLeaderMac) {
    return true;
  }
  return isSameLeaderMac;
}

} // namespace soarm
