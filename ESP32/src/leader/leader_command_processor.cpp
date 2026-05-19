#include "leader_command_processor.h"

#include <cstddef>

namespace soarm {

LeaderCommandProcessor::LeaderCommandProcessor() {
}

LeaderCommandAction LeaderCommandProcessor::process(const CommandFrame &frame) {
  if (!lockManager_.lock(LockDomain::Command)) {
    return LeaderCommandAction::None;
  }

  LeaderCommandAction action = LeaderCommandAction::None;

  if (frame.magic == kMagic && frame.version == kVersion) {
    static const LeaderCommandAction kActions[] = {
        LeaderCommandAction::None,
        LeaderCommandAction::StartStream,
        LeaderCommandAction::StopStream,
        LeaderCommandAction::Ping,
        LeaderCommandAction::ResetPairing,
        LeaderCommandAction::ServoScan,
        LeaderCommandAction::ServoDebugEnable,
        LeaderCommandAction::ServoDebugDisable,
        LeaderCommandAction::ServoMove,
      LeaderCommandAction::ServoSetId,
      LeaderCommandAction::ServoSetMode,
    };

    const size_t commandId = frame.command;
    if (commandId < (sizeof(kActions) / sizeof(kActions[0]))) {
      action = kActions[commandId];
    }
  }

  lockManager_.unlock(LockDomain::Command);
  return action;
}

} // namespace soarm
