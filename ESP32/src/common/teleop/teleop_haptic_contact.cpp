#include "teleop_haptic_contact.h"

#include "../../Config/leader_runtime_config.h"

namespace soarm {
namespace teleop_haptic {

int32_t gripperLeaderFollowerAbsGap(int16_t leaderPresent, int16_t followerOnLeader) {
  const int32_t delta = static_cast<int32_t>(leaderPresent) - static_cast<int32_t>(followerOnLeader);
  return delta < 0 ? -delta : delta;
}

bool shouldEngageGripperHaptic(uint8_t wireLoad, int32_t absPositionGap) {
  return shouldSyncGripperPosition(wireLoad) &&
         absPositionGap >= static_cast<int32_t>(config::leader::kTeleopHapticMinPositionGap);
}

bool shouldSyncGripperPosition(uint8_t wireLoad) {
  return wireLoad >= config::leader::kTeleopHapticPositionSyncMinWireLoad;
}

bool shouldDisengageMirrorCatchUp(uint8_t wireLoad, int32_t absPositionGap) {
  return absPositionGap < static_cast<int32_t>(config::leader::kTeleopHapticMirrorCatchUpGap) &&
         wireLoad < config::leader::kTeleopHapticPositionSyncMinWireLoad;
}

int16_t selectGripperHapticGoal(int16_t leaderPresent, int16_t followerOnLeader, uint8_t wireLoad) {
  return shouldSyncGripperPosition(wireLoad) ? followerOnLeader : leaderPresent;
}

} // namespace teleop_haptic
} // namespace soarm
