#pragma once

#include <cstdint>

namespace soarm {

enum class OledMenuScreenId : uint8_t {
  Root = 0,
  InfoDetail,
  TeleopList,
  CalibrationList,
  IkNotImplementedDetail,
  PairingList,
  PairingStatusDetail,
  PairingResetConfirm,
  Count,
};

constexpr uint8_t kOledMenuScreenCount = static_cast<uint8_t>(OledMenuScreenId::Count);

} // namespace soarm
