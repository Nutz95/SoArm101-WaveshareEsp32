#pragma once

#include "oled_menu_navigation_result.h"
#include "oled_menu_profile_selection.h"
#include "oled_menu_screen_id.h"

#include <cstdint>

namespace soarm {

enum class OledMenuTeleopItem : uint8_t {
  EspNow = 0,
  EspNowTurbo,
  Wifi,
  IkTeleop,
  Back,
  Count,
};

constexpr uint8_t kOledMenuTeleopItemCount = static_cast<uint8_t>(OledMenuTeleopItem::Count);

constexpr const char *kOledMenuTeleopLabels[kOledMenuTeleopItemCount] = {
    [static_cast<uint8_t>(OledMenuTeleopItem::EspNow)] = "ESP-NOW",
    [static_cast<uint8_t>(OledMenuTeleopItem::EspNowTurbo)] = "ESP-NOW Turbo",
    [static_cast<uint8_t>(OledMenuTeleopItem::Wifi)] = "Wi-Fi",
    [static_cast<uint8_t>(OledMenuTeleopItem::IkTeleop)] = "IK Teleop",
    [static_cast<uint8_t>(OledMenuTeleopItem::Back)] = "Back",
};

constexpr OledMenuNavigationResult kOledMenuTeleopItemActions[kOledMenuTeleopItemCount] = {
    [static_cast<uint8_t>(OledMenuTeleopItem::EspNow)] = OledMenuNavigationResult(
        OledMenuNavigationResult::Action::ActivateProfile, OledMenuScreenId::Root,
        OledMenuProfileSelection::TeleopEspNow),
    [static_cast<uint8_t>(OledMenuTeleopItem::EspNowTurbo)] = OledMenuNavigationResult(
        OledMenuNavigationResult::Action::ActivateProfile, OledMenuScreenId::Root,
        OledMenuProfileSelection::TeleopEspNowTurbo),
    [static_cast<uint8_t>(OledMenuTeleopItem::Wifi)] = OledMenuNavigationResult(
        OledMenuNavigationResult::Action::ActivateProfile, OledMenuScreenId::Root,
        OledMenuProfileSelection::TeleopWifi),
    [static_cast<uint8_t>(OledMenuTeleopItem::IkTeleop)] = OledMenuNavigationResult(
        OledMenuNavigationResult::Action::Push, OledMenuScreenId::IkNotImplementedDetail),
    [static_cast<uint8_t>(OledMenuTeleopItem::Back)] =
        OledMenuNavigationResult(OledMenuNavigationResult::Action::Pop, OledMenuScreenId::Root),
};

} // namespace soarm
