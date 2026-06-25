#pragma once

#include <cstdint>

namespace soarm {

constexpr uint8_t kOledMenuVisibleLines = 4U;
constexpr uint8_t kOledMenuLineChars = 22U;

// Four-line SSD1306 text buffer produced by menu screens.
struct OledMenuRenderOutput {
  char lines[kOledMenuVisibleLines][kOledMenuLineChars]{};
};

} // namespace soarm
