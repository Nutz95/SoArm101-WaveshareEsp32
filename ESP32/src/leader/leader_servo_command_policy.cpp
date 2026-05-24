#include "leader_servo_command_policy.h"

namespace soarm {

ServoSetIdRoutingDecision decideServoSetIdRouting(
    bool leaderDebugManualEnabled,
    bool followerDebugManualEnabled) {
  ServoSetIdRoutingDecision decision{};
  decision.executeLeaderLocal = leaderDebugManualEnabled;
  decision.forwardFollower = leaderDebugManualEnabled || followerDebugManualEnabled;
  return decision;
}

} // namespace soarm
