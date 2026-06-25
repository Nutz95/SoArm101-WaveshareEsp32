#pragma once

#include <cstdint>

namespace soarm {

// Operation profiles selectable from the OLED menu (Phase 2 leaves).
enum class OledMenuProfileSelection : uint8_t {
  TeleopEspNow = 0,
  TeleopEspNowTurbo,
  TeleopWifi,
  Passthrough,
};

} // namespace soarm
