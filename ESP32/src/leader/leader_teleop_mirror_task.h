#pragma once

#include "../common/interfaces/i_leader_presence_service.h"
#include "../common/types/calibration_profile.h"
#include "../common/servo/servo_bus_service.h"
#include "../common/teleop/teleop_transport_mode.h"
#include "../common/types/operation_mode.h"
#include "leader_teleop_wifi_bridge.h"

#include <atomic>
#include <cstdint>

namespace soarm {

struct TeleopMirrorLatencyMetrics {
  std::atomic<uint8_t> lastMs{0U};
  std::atomic<uint8_t> ewmaMs{0U};
  std::atomic<uint8_t> p95Ms{0U};
  std::atomic<uint8_t> pendingCount{0U};
  std::atomic<uint8_t> timeoutCount{0U};
};

class LeaderTeleopMirrorTask {
public:
  static void runLoop(
      ServoBusService &servoBusService,
      ILeaderPresenceService &presenceService,
      LeaderTeleopWifiBridge &teleopWifiBridge,
      const CalibrationProfile &leaderCalibrationProfile,
      const CalibrationProfile &followerCalibrationProfile,
      const std::atomic<bool> &continuousEnabled,
      const std::atomic<uint8_t> &servoIdFilter,
      const std::atomic<uint8_t> &speedPct,
      const std::atomic<uint8_t> &transportMode,
      const std::atomic<uint8_t> &runtimeMode,
      uint16_t &requestCounter,
      TeleopMirrorLatencyMetrics &latencyMetrics);
};

} // namespace soarm
