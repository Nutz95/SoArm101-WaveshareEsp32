#include "teleop_espnow_turbo_keyframe_policy.h"

#include "teleop_espnow_turbo_config.h"

namespace soarm {

bool TeleopEspNowTurboKeyframePolicy::shouldSendKeyframe(
    const TeleopEspNowTurboSession &session,
    uint32_t nowMs) {
  if (!session.hasBaselineKeyframe) {
    return true;
  }
  if (session.framesSinceKeyframe >= teleop_espnow::kTurboKeyframeEveryNFrames) {
    return true;
  }
  return (nowMs - session.lastKeyframeMs) >= teleop_espnow::kTurboKeyframeIntervalMs;
}

} // namespace soarm
