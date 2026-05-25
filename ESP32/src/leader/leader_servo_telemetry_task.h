#pragma once

#include "../common/servo/servo_bus_service.h"
#include "../common/types/operation_mode.h"

#include <atomic>

namespace soarm {

class LeaderServoTelemetryTask {
public:
  static void runLoop(
      ServoBusService &servoBusService,
      const std::atomic<bool> &continuousEnabled,
      const std::atomic<uint8_t> &runtimeMode);
};

} // namespace soarm
