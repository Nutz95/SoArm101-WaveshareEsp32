#include "leader_app.h"

#include "../Config/leader_runtime_config.h"

#include <array>

namespace soarm {

namespace {

struct ModeCycleBinding {
  uint8_t code;
  XboxLogicalButton button;
};

constexpr std::array<ModeCycleBinding, 6> kModeCycleBindings = {{
    {0U, XboxLogicalButton::None},
    {1U, XboxLogicalButton::View},
    {2U, XboxLogicalButton::Menu},
    {3U, XboxLogicalButton::Share},
    {4U, XboxLogicalButton::LeftStick},
    {5U, XboxLogicalButton::RightStick},
}};

bool tryDecodeModeCycleButton(uint8_t code, XboxLogicalButton &outButton) {
  for (const ModeCycleBinding &binding : kModeCycleBindings) {
    if (binding.code == code) {
      outButton = binding.button;
      return true;
    }
  }

  return false;
}

} // namespace

bool LeaderApp::handleXboxModeCycleButtonSetValueCommand() {
  uint32_t value = 0U;
  uint16_t requestId = 0U;
  if (!telemetryStreamServer_.consumeXboxModeCycleButtonSetRequested(value, requestId)) {
    return false;
  }

  beginCommandTracking(requestId, static_cast<uint8_t>(LeaderCommandAction::XboxModeCycleButtonSet));
  handleXboxModeCycleButtonSetCommand(value, requestId);
  return true;
}

void LeaderApp::handleXboxModeCycleButtonSetCommand(uint32_t value, uint16_t requestId) {
  (void)requestId;

  const uint8_t buttonValue = static_cast<uint8_t>(value & 0xFFU);
  XboxLogicalButton button = XboxLogicalButton::None;
  if (!tryDecodeModeCycleButton(buttonValue, button)) {
    setLeaderCommandStatus(CommandAckStatus::Rejected);
    setFollowerCommandStatus(CommandAckStatus::None);
    setTransientStatus("xbox mode cycle invalid", config::leader::kMoveStatusHoldMs);
    return;
  }

  xboxControllerService_.setModeCycleButton(button);
  setLeaderCommandStatus(CommandAckStatus::Applied);
  setFollowerCommandStatus(CommandAckStatus::None);
  setTransientStatus("xbox mode cycle button set", config::leader::kMoveStatusHoldMs);
}

} // namespace soarm
