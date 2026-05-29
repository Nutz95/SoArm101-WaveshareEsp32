#pragma once

#include "../../Config/common_runtime_config.h"

#include <cstdint>

namespace soarm {

struct ServoPositionSample {
  uint8_t id;
  int16_t position;
};

struct ServoPositionSnapshot {
  uint8_t count;
  uint32_t capturedAtMs;
  ServoPositionSample samples[config::common::kTeleopBatchMaxServos]{};
};

} // namespace soarm
