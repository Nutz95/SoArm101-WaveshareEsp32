#pragma once

namespace soarm {

class PairingPolicy {
public:
  static bool shouldAcceptLeaderPairRequest(bool hasPairedMac, bool isPairedMac);
  static bool shouldAcceptFollowerPairAck(bool hasPairedLeaderMac, bool isSameLeaderMac);
};

} // namespace soarm
