#pragma once

#include <cstdint>

namespace soarm {

// Operation profiles selectable from the OLED menu (Phase 2 leaves).
enum class OledMenuProfileSelection : uint8_t {
  TeleopEspNow = 0,
  TeleopEspNowTurbo,
  TeleopWifi,
  Passthrough,
  OtaReady,
  CalibrationLeader,
  CalibrationFollower,
  Count,
};

constexpr uint8_t kOledMenuProfileSelectionCount = static_cast<uint8_t>(OledMenuProfileSelection::Count);

} // namespace soarm
