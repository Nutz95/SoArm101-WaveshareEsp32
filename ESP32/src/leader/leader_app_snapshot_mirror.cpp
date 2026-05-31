#include "leader_app.h"

#include "../common/servo/servo_position_snapshot.h"

#include <climits>

namespace soarm {

void LeaderApp::fillLeaderMirrorPositions(LeaderTelemetrySnapshot &snapshot) {
  for (uint8_t i = 0U; i < CalibrationProfile::kServoCount; ++i) {
    snapshot.leaderMirrorPositions[i] = INT16_MIN;
  }
  snapshot.leaderMirrorPositionCount = 0U;

  ServoPositionSnapshot mirrorSnapshot{};
  if (!servoBusService_.copyPositionSnapshot(mirrorSnapshot)) {
    return;
  }

  snapshot.leaderMirrorPositionCount = mirrorSnapshot.count;
  for (uint8_t i = 0U; i < mirrorSnapshot.count; ++i) {
    const uint8_t id = mirrorSnapshot.samples[i].id;
    if (id >= 1U && id <= CalibrationProfile::kServoCount) {
      snapshot.leaderMirrorPositions[id - 1U] = mirrorSnapshot.samples[i].position;
    }
  }
}

} // namespace soarm
