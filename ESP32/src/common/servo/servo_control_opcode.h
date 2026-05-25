#pragma once

#include <cstdint>

namespace soarm {

enum class ServoControlOpcode : uint8_t {
  None = 0,
  Scan = 1,
  DebugEnable = 2,
  DebugDisable = 3,
  Move = 4,
  SetId = 5,
  SetMode = 6,
  TeleopMirror = 7
};

} // namespace soarm