#include "leader_command_processor.h"

namespace soarm {

LeaderCommandProcessor::LeaderCommandProcessor() {
}

LeaderCommandAction LeaderCommandProcessor::process(const CommandFrame &frame) {
  if (!lockManager_.lock(LockDomain::Command)) {
    return LeaderCommandAction::None;
  }

  LeaderCommandAction action = LeaderCommandAction::None;

  if (frame.magic == kMagic && frame.version == kVersion) {
    switch (frame.command) {
    case 1:
      action = LeaderCommandAction::StartStream;
      break;
    case 2:
      action = LeaderCommandAction::StopStream;
      break;
    case 3:
      action = LeaderCommandAction::Ping;
      break;
    case 4:
      action = LeaderCommandAction::ResetPairing;
      break;
    case 5:
      action = LeaderCommandAction::ServoScan;
      break;
    case 6:
      action = LeaderCommandAction::ServoDebugEnable;
      break;
    case 7:
      action = LeaderCommandAction::ServoDebugDisable;
      break;
    case 8:
      action = LeaderCommandAction::ServoMove;
      break;
    default:
      action = LeaderCommandAction::None;
      break;
    }
  }

  lockManager_.unlock(LockDomain::Command);
  return action;
}

} // namespace soarm
