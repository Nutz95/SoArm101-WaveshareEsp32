#pragma once

#include <cstdint>

namespace soarm {
namespace teleop_haptic {

/// Absolute STS gap between leader gripper present and follower gripper mapped onto leader calibration.
int32_t gripperLeaderFollowerAbsGap(int16_t leaderPresent, int16_t followerOnLeader);

/// True when follower load + leader/follower position mismatch indicate contact (not mirror chase).
bool shouldEngageGripperHaptic(uint8_t wireLoad, int32_t absPositionGap);

/// True when follower load is high enough to pull leader gripper to follower present (not mirror lag).
bool shouldSyncGripperPosition(uint8_t wireLoad);

/// True when the follower caught up with the leader — drop haptic before position feedback fights the user.
bool shouldDisengageMirrorCatchUp(uint8_t wireLoad, int32_t absPositionGap);

/// Haptic servo goal: follower pose on firm contact, else hold leader present (torque-only compliance).
int16_t selectGripperHapticGoal(int16_t leaderPresent, int16_t followerOnLeader, uint8_t wireLoad);

} // namespace teleop_haptic
} // namespace soarm
