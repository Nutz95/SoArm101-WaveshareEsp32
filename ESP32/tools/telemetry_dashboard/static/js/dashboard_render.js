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

function updateLeaderDebugToggleButton(debugEnabled) {
  const toggleBtn = document.getElementById("leaderServoDebugToggleBtn");
  if (!toggleBtn) {
    return;
  }

  toggleBtn.dataset.enabled = debugEnabled ? "1" : "0";
  toggleBtn.classList.toggle("state-on", !!debugEnabled);
  toggleBtn.textContent = debugEnabled
    ? "Disable Debug Manual (Leader)"
    : "Enable Debug Manual (Leader)";
}

function xboxRuntimeLabel(stateValue) {
  const state = Number(stateValue || 0);
  if (state === 1) {
    return "scanning";
  }
  if (state === 2) {
    return "pairing";
  }
  if (state === 3) {
    return "connected";
  }
  return "disconnected";
}

function updateXboxRuntime(data) {
  const runtimeLabel = xboxRuntimeLabel(data.xbox_runtime_state);
  const paired = !!data.xbox_controller_paired;
  const encrypted = !!data.xbox_link_encrypted;
  const subscribed = !!data.xbox_input_subscribed;
  const ageMs = Number(data.xbox_last_report_age_ms || 0);
  const reportCount = Number(data.xbox_report_count || 0);
  const buttonsMask = Number(data.xbox_buttons_mask || 0) & 0xFFFF;
  const leftX = Number(data.xbox_axis_left_x || 0);
  const leftY = Number(data.xbox_axis_left_y || 0);
  const rightX = Number(data.xbox_axis_right_x || 0);
  const rightY = Number(data.xbox_axis_right_y || 0);
  const triggerLeft = Number(data.xbox_trigger_left || 0);
  const triggerRight = Number(data.xbox_trigger_right || 0);
  const name = data.xbox_controller_name || "-";

  const runtimeStateNode = document.getElementById("xboxRuntimeState");
  if (runtimeStateNode) {
    runtimeStateNode.textContent = runtimeLabel;
  }

  const runtimeAgeNode = document.getElementById("xboxRuntimeAge");
  if (runtimeAgeNode) {
    runtimeAgeNode.textContent = `${ageMs} ms`;
  }

  const runtimeNameNode = document.getElementById("xboxRuntimeController");
  if (runtimeNameNode) {
    runtimeNameNode.textContent = name;
  }

  const runtimePairNode = document.getElementById("xboxRuntimePairing");
  if (runtimePairNode) {
    runtimePairNode.textContent = paired ? "paired" : "not paired";
  }

  const runtimeEncryptedNode = document.getElementById("xboxRuntimeEncrypted");
  if (runtimeEncryptedNode) {
    runtimeEncryptedNode.textContent = encrypted ? "on" : "off";
  }

  const runtimeSubscribedNode = document.getElementById("xboxRuntimeSubscribed");
  if (runtimeSubscribedNode) {
    runtimeSubscribedNode.textContent = subscribed ? "on" : "off";
  }

  const reportCountNode = document.getElementById("xboxRuntimeReportCount");
  if (reportCountNode) {
    reportCountNode.textContent = `${reportCount}`;
  }

  const buttonsNode = document.getElementById("xboxButtonsMask");
  if (buttonsNode) {
    buttonsNode.textContent = `0x${buttonsMask.toString(16).padStart(4, "0")}`;
  }

  const leftStickNode = document.getElementById("xboxLeftStick");
  if (leftStickNode) {
    leftStickNode.textContent = `${leftX}, ${leftY}`;
  }

  const rightStickNode = document.getElementById("xboxRightStick");
  if (rightStickNode) {
    rightStickNode.textContent = `${rightX}, ${rightY}`;
  }

  const triggerNode = document.getElementById("xboxTriggers");
  if (triggerNode) {
    triggerNode.textContent = `${triggerLeft}, ${triggerRight}`;
  }

  const runtimeDot = document.getElementById("xboxRuntimeDot");
  if (runtimeDot) {
    runtimeDot.classList.remove("state-green", "state-red", "state-amber");
    if (runtimeLabel === "connected" && paired && encrypted && subscribed) {
      runtimeDot.classList.add("state-green");
    } else if (runtimeLabel === "scanning" || runtimeLabel === "pairing") {
      runtimeDot.classList.add("state-amber");
    } else {
      runtimeDot.classList.add("state-red");
    }
  }
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
  document.getElementById("leaderTempAlarm").textContent = data.leader_servo_temperature_alarm ? "on" : "off";
  document.getElementById("followerTempAlarm").textContent = data.follower_servo_temperature_alarm ? "on" : "off";
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
  updateLeaderDebugToggleButton(!!data.leader_servo_debug_manual);
  updateFollowerDebugButtons(!!data.follower_servo_debug_manual);
  updateXboxRuntime(data);

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
