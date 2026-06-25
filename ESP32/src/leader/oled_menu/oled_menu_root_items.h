#pragma once

#include "oled_menu_navigation_result.h"
#include "oled_menu_profile_selection.h"
#include "oled_menu_screen_id.h"

#include <cstdint>

namespace soarm {

constexpr uint8_t kOledMenuRootItemCount = 6U;
constexpr uint8_t kOledMenuRootInfoIndex = 0U;
constexpr uint8_t kOledMenuRootTeleopIndex = 1U;
constexpr uint8_t kOledMenuRootPassthroughIndex = 2U;
constexpr uint8_t kOledMenuRootCalibrationIndex = 3U;
constexpr uint8_t kOledMenuRootPairingIndex = 4U;
constexpr uint8_t kOledMenuRootOtaIndex = 5U;

constexpr uint8_t kOledMenuPairingItemCount = 2U;
constexpr uint8_t kOledMenuPairingStatusIndex = 0U;
constexpr uint8_t kOledMenuPairingBackIndex = 1U;

constexpr const char *kOledMenuRootLabels[kOledMenuRootItemCount] = {
    "Info",
    "Teleop",
    "Passthrough",
    "Calibration",
    "Pairing",
    "OTA",
};

constexpr const char *kOledMenuPairingLabels[kOledMenuPairingItemCount] = {
    "Status",
    "Back",
};

constexpr OledMenuNavigationResult kOledMenuRootItemActions[kOledMenuRootItemCount] = {
    OledMenuNavigationResult(OledMenuNavigationResult::Action::Push, OledMenuScreenId::InfoDetail),
    OledMenuNavigationResult(OledMenuNavigationResult::Action::Push, OledMenuScreenId::TeleopList),
    OledMenuNavigationResult(OledMenuNavigationResult::Action::ActivateProfile, OledMenuScreenId::Root,
                             OledMenuProfileSelection::Passthrough),
    OledMenuNavigationResult(OledMenuNavigationResult::Action::None, OledMenuScreenId::Root),
    OledMenuNavigationResult(OledMenuNavigationResult::Action::Push, OledMenuScreenId::PairingList),
    OledMenuNavigationResult(OledMenuNavigationResult::Action::ActivateProfile, OledMenuScreenId::Root,
                             OledMenuProfileSelection::OtaReady),
};

} // namespace soarm
