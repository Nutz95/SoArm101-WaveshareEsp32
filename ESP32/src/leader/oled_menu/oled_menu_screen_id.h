#pragma once

#include <cstdint>

namespace soarm {

enum class OledMenuScreenId : uint8_t {
  Root = 0,
  InfoDetail,
  TeleopList,
  IkNotImplementedDetail,
  PairingList,
  PairingStatusDetail,
};

} // namespace soarm
