#pragma once

#include "../common/servo/servo_bus_service.h"

#include <atomic>

namespace soarm {

class LeaderServoTelemetryTask {
public:
  static void runLoop(ServoBusService &servoBusService, const std::atomic<bool> &continuousEnabled);
};

} // namespace soarm
