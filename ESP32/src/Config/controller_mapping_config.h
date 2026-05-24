#pragma once

#include <cstddef>
#include <cstdint>

namespace soarm {
namespace config {
namespace controller {

// Current baseline mapping imported from AutoBalancingAIBot for Xbox reports.
constexpr const char *kPreferredControllerName = "Xbox Wireless Controller";
constexpr size_t kMinHidReportLength = 16U;

// Mode cycle button: each press advances to the next implemented operation mode
// (currently: Idle -> Teleoperation -> Idle -> ...).
constexpr uint8_t kModeCycleButtonByte = 15U;
constexpr uint8_t kModeCycleButtonMask = 0x01U;

// User action button: purpose TBD per application; placeholder for secondary action.
constexpr uint8_t kUserActionButtonByte = 14U;
constexpr uint8_t kUserActionButtonMask = 0x08U;

// Reserved IK control plan.
// Left stick: X/Y gripper translation.
// Right stick: Z translation + wrist rotation.
// Triggers: LT/RT gripper close/open.
constexpr uint8_t kLeftStickXLowByte = 0U;
constexpr uint8_t kLeftStickXHighByte = 1U;
constexpr uint8_t kLeftStickYLowByte = 2U;
constexpr uint8_t kLeftStickYHighByte = 3U;

} // namespace controller
} // namespace config
} // namespace soarm
