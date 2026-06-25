#pragma once

#include "oled_menu_navigation_result.h"
#include "oled_menu_screen_id.h"

#include <cstdint>

namespace soarm {

enum class OledMenuPairingItem : uint8_t {
  Status = 0,
  Back,
  Count,
};

constexpr uint8_t kOledMenuPairingItemCount = static_cast<uint8_t>(OledMenuPairingItem::Count);

constexpr const char *kOledMenuPairingLabels[kOledMenuPairingItemCount] = {
    [static_cast<uint8_t>(OledMenuPairingItem::Status)] = "Status",
    [static_cast<uint8_t>(OledMenuPairingItem::Back)] = "Back",
};

constexpr OledMenuNavigationResult kOledMenuPairingItemActions[kOledMenuPairingItemCount] = {
    [static_cast<uint8_t>(OledMenuPairingItem::Status)] =
        OledMenuNavigationResult(OledMenuNavigationResult::Action::Push,
                                 OledMenuScreenId::PairingStatusDetail),
    [static_cast<uint8_t>(OledMenuPairingItem::Back)] =
        OledMenuNavigationResult(OledMenuNavigationResult::Action::Pop, OledMenuScreenId::Root),
};

} // namespace soarm
