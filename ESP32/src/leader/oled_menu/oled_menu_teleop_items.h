#pragma once

#include "oled_menu_navigation_result.h"
#include "oled_menu_profile_selection.h"
#include "oled_menu_screen_id.h"

#include <cstdint>

namespace soarm {

constexpr uint8_t kOledMenuTeleopItemCount = 5U;
constexpr uint8_t kOledMenuTeleopEspNowIndex = 0U;
constexpr uint8_t kOledMenuTeleopEspNowTurboIndex = 1U;
constexpr uint8_t kOledMenuTeleopWifiIndex = 2U;
constexpr uint8_t kOledMenuTeleopIkIndex = 3U;
constexpr uint8_t kOledMenuTeleopBackIndex = 4U;

constexpr const char *kOledMenuTeleopLabels[kOledMenuTeleopItemCount] = {
    "ESP-NOW",
    "ESP-NOW Turbo",
    "Wi-Fi",
    "IK Teleop",
    "Back",
};

constexpr OledMenuNavigationResult kOledMenuTeleopItemActions[kOledMenuTeleopItemCount] = {
    OledMenuNavigationResult(OledMenuNavigationResult::Action::ActivateProfile, OledMenuScreenId::Root,
                             OledMenuProfileSelection::TeleopEspNow),
    OledMenuNavigationResult(OledMenuNavigationResult::Action::ActivateProfile, OledMenuScreenId::Root,
                             OledMenuProfileSelection::TeleopEspNowTurbo),
    OledMenuNavigationResult(OledMenuNavigationResult::Action::ActivateProfile, OledMenuScreenId::Root,
                             OledMenuProfileSelection::TeleopWifi),
    OledMenuNavigationResult(OledMenuNavigationResult::Action::Push,
                             OledMenuScreenId::IkNotImplementedDetail),
    OledMenuNavigationResult(OledMenuNavigationResult::Action::Pop, OledMenuScreenId::Root),
};

} // namespace soarm
