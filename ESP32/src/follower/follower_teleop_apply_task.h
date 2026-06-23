#pragma once

#include "../Config/common_runtime_config.h"

#include <cstdint>

namespace soarm {

class FollowerApp;

// Dedicated ~60 Hz task: ingest latest teleop batch from all transports, then moveBatch only.
class FollowerTeleopApplyTask {
public:
  struct TeleopFrame {
    uint8_t ids[config::common::kTeleopBatchMaxServos]{};
    int16_t positions[config::common::kTeleopBatchMaxServos]{};
    uint8_t count{0U};
    uint8_t speedPercent{0U};
    uint16_t requestId{0U};
    uint8_t flags{0U};
    bool turbo{false};
    bool valid{false};
  };

  static void runLoop(FollowerApp &app);

private:
  static bool ingestLatestWifi(FollowerApp &app, TeleopFrame &out);
  static bool ingestLatestEspNow(FollowerApp &app, TeleopFrame &out);
  static const TeleopFrame *selectFrame(const TeleopFrame &wifi, const TeleopFrame &espNow);
};

} // namespace soarm
