#include "leader_app.h"

#include "../Config/common_runtime_config.h"

namespace soarm {

void LeaderApp::updateServoHealthFlags() {
  leaderServoFault_ = servoBusService_.lastScanCount() != config::common::kExpectedLeaderServoCount;

  if (!presenceService_->isFollowerLinked() || teleopContinuousEnabled_.load()) {
    followerServoFault_ = false;
    return;
  }

  const uint8_t followerCount = presenceService_->followerServoCount();
  followerServoFault_ =
      followerCount != 0U && followerCount != config::common::kExpectedFollowerServoCount;
}

} // namespace soarm
