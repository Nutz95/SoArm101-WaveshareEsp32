#include "leader_app.h"

#include <cstring>

namespace soarm {

void LeaderApp::buildOledModeLine(char *buffer, size_t bufferSize) const {
  if (buffer == nullptr || bufferSize == 0U) {
    return;
  }

  strncpy(buffer, "Mode: IDLE", bufferSize - 1U);
  buffer[bufferSize - 1U] = '\0';

  if (mode_ == OperationMode::CalibrationLeader || mode_ == OperationMode::CalibrationFollower) {
    strncpy(buffer, "Mode: CALIBRATION", bufferSize - 1U);
    buffer[bufferSize - 1U] = '\0';
    return;
  }

  if (mode_ != OperationMode::Teleoperation) {
    return;
  }

  if (static_cast<TeleopTransportMode>(teleopTransportMode_.load()) == TeleopTransportMode::WifiUdp) {
    strncpy(buffer, "Mode: TELEOP WIFI", bufferSize - 1U);
  } else {
    strncpy(buffer, "Mode: TELEOP ESPNOW", bufferSize - 1U);
  }
  buffer[bufferSize - 1U] = '\0';
}

} // namespace soarm
