#pragma once

namespace soarm {

struct ServoSetIdRoutingDecision {
  bool executeLeaderLocal;
  bool forwardFollower;
};

ServoSetIdRoutingDecision decideServoSetIdRouting(
    bool leaderDebugManualEnabled,
    bool followerDebugManualEnabled);

} // namespace soarm
