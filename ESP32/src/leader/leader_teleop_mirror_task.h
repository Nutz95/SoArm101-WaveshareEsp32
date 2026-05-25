#pragma once

#include "../common/interfaces/i_leader_presence_service.h"
#include "../common/servo/servo_bus_service.h"

#include <atomic>
#include <cstdint>

namespace soarm {

class LeaderTeleopMirrorTask {
public:
  static void runLoop(
      ServoBusService &servoBusService,
      ILeaderPresenceService &presenceService,
      const std::atomic<bool> &continuousEnabled,
      const std::atomic<uint8_t> &servoIdFilter,
      uint16_t &requestCounter);
};

} // namespace soarm
