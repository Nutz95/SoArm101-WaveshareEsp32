import { commandAckLabel, isTerminalAck } from "./ack_status.js";

const COMMAND_PENDING_TIMEOUT_MS = 6500;

let pendingCommand = null;

function nowMs() {
  return Date.now();
}

function setFollowerDebugButtonsDisabled(disabled) {
  const enableBtn = document.getElementById("followerServoDebugEnableBtn");
  const disableBtn = document.getElementById("followerServoDebugDisableBtn");
  if (!enableBtn || !disableBtn) {
    return;
  }
  enableBtn.disabled = disabled;
  disableBtn.disabled = disabled;
}

function clearPendingCommand() {
  if (pendingCommand?.scope === "follower") {
    setFollowerDebugButtonsDisabled(false);
  }
  pendingCommand = null;
}

export function hasPendingFollowerCommand() {
  return pendingCommand?.scope === "follower";
}

export function registerPendingCommand(result, scope, label) {
  if (!result || !result.ok || !result.request_id) {
    return;
  }

  pendingCommand = {
    requestId: result.request_id | 0,
    scope,
    label,
    startedAtMs: nowMs(),
  };

  if (scope === "follower") {
    setFollowerDebugButtonsDisabled(true);
  }

  const statusNode = document.getElementById("commandStatus");
  if (statusNode) {
    statusNode.textContent = `${label} pending (req ${pendingCommand.requestId})`;
  }
}

export function syncPendingCommandStatus(data) {
  if (!pendingCommand) {
    return;
  }

  const statusNode = document.getElementById("commandStatus");
  const elapsedMs = nowMs() - pendingCommand.startedAtMs;
  if (elapsedMs > COMMAND_PENDING_TIMEOUT_MS) {
    statusNode.textContent = `${pendingCommand.label} pending timeout (req ${pendingCommand.requestId})`;
    clearPendingCommand();
    return;
  }

  if ((data.command_request_id | 0) !== pendingCommand.requestId) {
    return;
  }

  const leaderStatus = data.leader_command_status | 0;
  const followerStatus = data.follower_command_status | 0;
  const observedStatus = pendingCommand.scope === "follower" ? followerStatus : leaderStatus;
  if (!isTerminalAck(observedStatus)) {
    statusNode.textContent = `${pendingCommand.label} pending (${commandAckLabel(observedStatus)}) (req ${pendingCommand.requestId})`;
    return;
  }

  statusNode.textContent = `${pendingCommand.label} ${commandAckLabel(observedStatus)} (req ${pendingCommand.requestId})`;
  clearPendingCommand();
}
