#pragma once

#include <cstdint>

namespace soarm {

enum class OledTextStyle : uint8_t {
  Small = 0,
  Medium,
  Large
};

// Central place to tweak OLED rendering behavior.
// You can tune these values from LeaderApp constructor.
struct OledDisplayConfig {
  OledDisplayConfig() = default;
  OledDisplayConfig(uint8_t width,
                    uint8_t height,
                    uint8_t address,
                    uint16_t refreshMs,
                    bool alternate,
                    OledTextStyle style)
      : screenWidth(width),
        screenHeight(height),
        i2cAddress(address),
        refreshPeriodMs(refreshMs),
        alternatePages(alternate),
        textStyle(style) {}

  uint8_t screenWidth{128};
  uint8_t screenHeight{32};
  uint8_t i2cAddress{0x3C};

  uint16_t refreshPeriodMs{300};
  bool alternatePages{false};

  // Small: built-in 5x7 font (setTextSize(1))
  // Medium: FreeSans 7pt (intermediate when height allows it)
  // Large: built-in doubled font (setTextSize(2))
  OledTextStyle textStyle{OledTextStyle::Small};
};

} // namespace soarm
