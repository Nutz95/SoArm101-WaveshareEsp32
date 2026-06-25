#pragma once

#include "oled_menu_navigation_result.h"
#include "oled_menu_profile_selection.h"
#include "oled_menu_screen_id.h"

#include <cstdint>

namespace soarm {

enum class OledMenuCalibrationItem : uint8_t {
  Leader = 0,
  Follower,
  Back,
  Count,
};

constexpr uint8_t kOledMenuCalibrationItemCount = static_cast<uint8_t>(OledMenuCalibrationItem::Count);

constexpr const char *kOledMenuCalibrationLabels[kOledMenuCalibrationItemCount] = {
    [static_cast<uint8_t>(OledMenuCalibrationItem::Leader)] = "Leader",
    [static_cast<uint8_t>(OledMenuCalibrationItem::Follower)] = "Follower",
    [static_cast<uint8_t>(OledMenuCalibrationItem::Back)] = "Back",
};

constexpr OledMenuNavigationResult kOledMenuCalibrationItemActions[kOledMenuCalibrationItemCount] = {
    [static_cast<uint8_t>(OledMenuCalibrationItem::Leader)] = OledMenuNavigationResult(
        OledMenuNavigationResult::Action::ActivateProfile, OledMenuScreenId::Root,
        OledMenuProfileSelection::CalibrationLeader),
    [static_cast<uint8_t>(OledMenuCalibrationItem::Follower)] = OledMenuNavigationResult(
        OledMenuNavigationResult::Action::ActivateProfile, OledMenuScreenId::Root,
        OledMenuProfileSelection::CalibrationFollower),
    [static_cast<uint8_t>(OledMenuCalibrationItem::Back)] =
        OledMenuNavigationResult(OledMenuNavigationResult::Action::Pop, OledMenuScreenId::Root),
};

} // namespace soarm
