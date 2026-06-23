#pragma once

#include "../../Config/common_runtime_config.h"

#include <cstdint>

namespace soarm {

struct TeleopEspNowBatchPayload {
  uint8_t count{0U};
  uint8_t speedPct{0U};
  uint16_t requestId{0U};
  bool turbo{false};
  uint8_t ids[config::common::kTeleopBatchMaxServos]{};
  int16_t positions[config::common::kTeleopBatchMaxServos]{};
};

} // namespace soarm
