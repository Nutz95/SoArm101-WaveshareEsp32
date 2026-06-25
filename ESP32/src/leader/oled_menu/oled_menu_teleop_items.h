#pragma once

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

} // namespace soarm
