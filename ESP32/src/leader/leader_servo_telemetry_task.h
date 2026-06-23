#pragma once

#include "../common/controller/controller_operation_profile.h"
#include "../common/servo/servo_bus_service.h"
#include "../common/types/operation_mode.h"

#include <atomic>

namespace soarm {

class LeaderServoTelemetryTask {
public:
  static void runLoop(
      ServoBusService &servoBusService,
      const std::atomic<bool> &continuousEnabled,
      const std::atomic<uint8_t> &runtimeMode,
      const std::atomic<uint8_t> &controllerOperationProfile);
};

} // namespace soarm
