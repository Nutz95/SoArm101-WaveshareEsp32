#pragma once

#include "teleop_espnow_turbo_session.h"

#include <cstdint>

namespace soarm {

/// Decides when the turbo codec must send a full keyframe instead of a sparse delta.
class TeleopEspNowTurboKeyframePolicy {
public:
  /// True on first frame, every N frames, or when the keyframe interval elapsed.
  static bool shouldSendKeyframe(const TeleopEspNowTurboSession &session, uint32_t nowMs);
};

} // namespace soarm
