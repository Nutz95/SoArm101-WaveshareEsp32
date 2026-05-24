import { modeLabel, stateLabel, uptimeLabel, boolLabel } from "./formatters.js";
import { renderServoChips } from "./servo_ui.js";
import { commandAckLabel } from "./ack_status.js";

function updateFollowerDebugButtons(debugEnabled) {
  const enableBtn = document.getElementById("followerServoDebugEnableBtn");
  const disableBtn = document.getElementById("followerServoDebugDisableBtn");
  if (!enableBtn || !disableBtn) {
    return;
  }

  enableBtn.classList.toggle("state-on", !!debugEnabled);
  disableBtn.classList.toggle("state-on", !debugEnabled);
}

export function renderSnapshot(data) {
  document.getElementById("connected").textContent = data.connected ? "yes" : "no";
  document.getElementById("leaderIp").textContent = data.leader_ip || "-";
  document.getElementById("followerIp").textContent = data.follower_ip || "-";
  document.getElementById("mode").textContent = modeLabel(data.mode);
  document.getElementById("leaderState").textContent = stateLabel(data.leader_state);
  document.getElementById("followerState").textContent = stateLabel(data.follower_state);
  document.getElementById("status").textContent = data.status || "-";
  document.getElementById("cpu0").textContent = `${data.cpu0_load_pct}%`;
  document.getElementById("cpu1").textContent = `${data.cpu1_load_pct}%`;
  document.getElementById("uptime").textContent = uptimeLabel(data.uptime_ms);
  document.getElementById("pairingLocked").textContent = data.pairing_locked ? "locked" : "open";
  document.getElementById("pairingLocked").className = data.pairing_locked ? "locked" : "";
  document.getElementById("leaderMac").textContent = data.leader_mac || "-";
  document.getElementById("followerMac").textContent = data.follower_mac || "-";

  document.getElementById("leaderServoCount").textContent = `${data.leader_servo_count || 0}`;
  document.getElementById("followerServoCount").textContent = `${data.follower_servo_count || 0}`;
  document.getElementById("leaderServoDebug").textContent = boolLabel(!!data.leader_servo_debug_manual);
  document.getElementById("followerServoDebug").textContent = boolLabel(!!data.follower_servo_debug_manual);
  document.getElementById("leaderServoTelemetry").textContent = data.leader_servo_telemetry || "-";
  document.getElementById("followerServoTelemetry").textContent = data.follower_servo_telemetry || "-";
  updateFollowerDebugButtons(!!data.follower_servo_debug_manual);

  document.getElementById("cmdRequestId").textContent = `${data.command_request_id || 0}`;
  document.getElementById("cmdCode").textContent = `${data.command_code || 0}`;
  document.getElementById("cmdLeaderAck").textContent = commandAckLabel(data.leader_command_status);
  document.getElementById("cmdFollowerAck").textContent = commandAckLabel(data.follower_command_status);
  document.getElementById("cmdFollowerRetries").textContent = `${data.follower_ack_retries_used || 0}`;
  document.getElementById("cmdFollowerRtt").textContent = `${data.follower_ack_rtt_ms || 0} ms`;

  document.getElementById("diagFollowerAckPending").textContent = data.follower_ack_pending ? "yes" : "no";
  document.getElementById("diagFollowerTimeoutCount").textContent = `${data.follower_ack_timeout_count || 0}`;
  document.getElementById("diagFollowerRtt").textContent = `${data.follower_ack_rtt_ms || 0} ms`;
  document.getElementById("diagFollowerRetries").textContent = `${data.follower_ack_retries_used || 0}`;

  renderServoChips("leaderServoIds", data.leader_servo_ids);
  renderServoChips("followerServoIds", data.follower_servo_ids);
}
