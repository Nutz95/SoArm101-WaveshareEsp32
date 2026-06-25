#pragma once

#include "oled_menu_navigation_result.h"
#include "oled_menu_profile_selection.h"
#include "oled_menu_screen_id.h"

#include <cstdint>

namespace soarm {

enum class OledMenuRootItem : uint8_t {
  Info = 0,
  Teleop,
  Passthrough,
  Calibration,
  Pairing,
  Ota,
  Count,
};

constexpr uint8_t kOledMenuRootItemCount = static_cast<uint8_t>(OledMenuRootItem::Count);

constexpr const char *kOledMenuRootLabels[kOledMenuRootItemCount] = {
    [static_cast<uint8_t>(OledMenuRootItem::Info)] = "Info",
    [static_cast<uint8_t>(OledMenuRootItem::Teleop)] = "Teleop",
    [static_cast<uint8_t>(OledMenuRootItem::Passthrough)] = "Passthrough",
    [static_cast<uint8_t>(OledMenuRootItem::Calibration)] = "Calibration",
    [static_cast<uint8_t>(OledMenuRootItem::Pairing)] = "Pairing",
    [static_cast<uint8_t>(OledMenuRootItem::Ota)] = "OTA",
};

constexpr OledMenuNavigationResult kOledMenuRootItemActions[kOledMenuRootItemCount] = {
    [static_cast<uint8_t>(OledMenuRootItem::Info)] =
        OledMenuNavigationResult(OledMenuNavigationResult::Action::Push, OledMenuScreenId::InfoDetail),
    [static_cast<uint8_t>(OledMenuRootItem::Teleop)] =
        OledMenuNavigationResult(OledMenuNavigationResult::Action::Push, OledMenuScreenId::TeleopList),
    [static_cast<uint8_t>(OledMenuRootItem::Passthrough)] = OledMenuNavigationResult(
        OledMenuNavigationResult::Action::ActivateProfile, OledMenuScreenId::Root,
        OledMenuProfileSelection::Passthrough),
    [static_cast<uint8_t>(OledMenuRootItem::Calibration)] =
        OledMenuNavigationResult(OledMenuNavigationResult::Action::Push, OledMenuScreenId::CalibrationList),
    [static_cast<uint8_t>(OledMenuRootItem::Pairing)] =
        OledMenuNavigationResult(OledMenuNavigationResult::Action::Push, OledMenuScreenId::PairingList),
    [static_cast<uint8_t>(OledMenuRootItem::Ota)] = OledMenuNavigationResult(
        OledMenuNavigationResult::Action::ActivateProfile, OledMenuScreenId::Root,
        OledMenuProfileSelection::OtaReady),
};

} // namespace soarm
