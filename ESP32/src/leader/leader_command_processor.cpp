#include "leader_command_processor.h"

#include <cstddef>

namespace {

struct CommandEntry {
  uint8_t id;
  soarm::LeaderCommandAction action;
};

constexpr CommandEntry kCommandEntries[] = {
    {1U, soarm::LeaderCommandAction::StartStream},
    {2U, soarm::LeaderCommandAction::StopStream},
    {3U, soarm::LeaderCommandAction::Ping},
    {4U, soarm::LeaderCommandAction::ResetPairing},
    {5U, soarm::LeaderCommandAction::ServoScan},
    {6U, soarm::LeaderCommandAction::ServoDebugEnable},
    {7U, soarm::LeaderCommandAction::ServoDebugDisable},
    {8U, soarm::LeaderCommandAction::ServoMove},
    {9U, soarm::LeaderCommandAction::ServoSetId},
    {10U, soarm::LeaderCommandAction::ServoSetMode},
    {11U, soarm::LeaderCommandAction::ServoDebugEnableFollower},
    {12U, soarm::LeaderCommandAction::ServoDebugDisableFollower},
    {13U, soarm::LeaderCommandAction::ServoScanLeader},
    {14U, soarm::LeaderCommandAction::ServoScanFollower},
    {15U, soarm::LeaderCommandAction::TeleopMirror},
    {16U, soarm::LeaderCommandAction::TeleopContinuousSet},
};

} // namespace

namespace soarm {

LeaderCommandProcessor::LeaderCommandProcessor() {
}

LeaderCommandAction LeaderCommandProcessor::process(const CommandFrame &frame) {
  if (!lockManager_.lock(LockDomain::Command)) {
    return LeaderCommandAction::None;
  }

  LeaderCommandAction action = LeaderCommandAction::None;

  if (frame.magic == kMagic && frame.version == kVersion) {
    for (size_t i = 0; i < (sizeof(kCommandEntries) / sizeof(kCommandEntries[0])); ++i) {
      if (kCommandEntries[i].id == frame.command) {
        action = kCommandEntries[i].action;
        break;
      }
    }
  }

  lockManager_.unlock(LockDomain::Command);
  return action;
}

} // namespace soarm
