#pragma once

#include <cstdint>

namespace soarm {

enum class TeleopTransportMode : uint8_t {
  EspNow = 0U,
  WifiUdp = 1U,
  PcSerialBridge = 2U,
};

} // namespace soarm
