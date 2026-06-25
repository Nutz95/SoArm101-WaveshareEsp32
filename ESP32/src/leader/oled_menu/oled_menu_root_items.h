#pragma once

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

} // namespace soarm
