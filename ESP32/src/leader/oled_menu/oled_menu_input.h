#pragma once

#include <cstdint>

namespace soarm {

// Xbox-driven menu navigation events (see docs/oled_menu_refactor_plan.md).
enum class OledMenuInputEvent : uint8_t {
  NavigateUp = 0,
  NavigateDown,
  Select,
  Back,
  ModeDown,
};

} // namespace soarm
