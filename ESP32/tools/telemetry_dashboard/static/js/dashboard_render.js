import { modeLabel, stateLabel, uptimeLabel, boolLabel } from "./formatters.js";
import { renderServoChips } from "./servo_ui.js";
import { commandAckLabel } from "./ack_status.js";
import { calibrationInstruction, renderCalibrationTable } from "./calibration_render.js";

const calibrationProfileByMode = {
  0: "calibration_leader",
  1: "calibration_follower",
  2: "teleop_espnow",
  3: "teleop_wifi",
};


function updateFollowerDebugButtons(debugEnabled) {
  const toggleBtn = document.getElementById("followerServoDebugToggleBtn");
  if (!toggleBtn) {
    return;
  }

  toggleBtn.dataset.enabled = debugEnabled ? "1" : "0";
  toggleBtn.classList.toggle("state-on", !!debugEnabled);
  toggleBtn.textContent = debugEnabled
    ? "Disable Debug Manual (Follower)"
    : "Enable Debug Manual (Follower)";
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

function setControlState(id, pressed) {
  const node = document.getElementById(id);
  if (!node) {
    return;
  }

  node.classList.toggle("is-active", !!pressed);
}

function servoPositionsSummary(telemetryText) {
  const source = String(telemetryText || "");
  const matches = [...source.matchAll(/#(\d+)\s+p(-?\d+)/g)];
  if (!matches.length) {
    return "-";
  }

  return matches.map((entry) => `S${entry[1]}:${entry[2]}`).join(" | ");
}

function parseServoPositions(telemetryText) {
  const source = String(telemetryText || "");
  return [...source.matchAll(/#(\d+)\s+p(-?\d+)/g)].map((entry) => ({
    id: Number(entry[1]),
    position: Number(entry[2]),
  }));
}

function formatCalibrationProfile(modeValue) {
  const numericMode = Number(modeValue || 0);
  return calibrationProfileByMode[numericMode] || "idle";
}

function renderCalibrationPanel(data) {
  const profileValue = Number(data.controller_operation_profile ?? 2);
  const activeRole = profileValue === 0 ? "leader" : profileValue === 1 ? "follower" : "";
  const captureActive = Number(data.calibration_phase || 0) !== 0;
  const selectedRoleNode = document.getElementById("calibrationModeSelect");
  const selectedRole = selectedRoleNode?.value === "calibration_follower" ? "follower" : "leader";
  const displayRole = activeRole || selectedRole;
  const telemetryText = displayRole === "follower" ? data.follower_servo_telemetry : data.leader_servo_telemetry;
  const rows = parseServoPositions(telemetryText);

  const calibrationProfile = displayRole === "follower"
    ? {
        min: captureActive ? (data.follower_working_calibration_min || []) : (data.follower_calibration_min || []),
        max: captureActive ? (data.follower_working_calibration_max || []) : (data.follower_calibration_max || []),
      }
    : {
        min: captureActive ? (data.leader_working_calibration_min || []) : (data.leader_calibration_min || []),
        max: captureActive ? (data.leader_working_calibration_max || []) : (data.leader_calibration_max || []),
      };

  const currentNode = document.getElementById("pairingModeCurrent");
  if (currentNode) {
    currentNode.textContent = formatCalibrationProfile(profileValue);
  }

  const pairingSelect = document.getElementById("pairingModeSelect");
  if (pairingSelect && document.activeElement !== pairingSelect && profileValue >= 2) {
    // Only update the select when in Teleoperation mode; use transport_mode to distinguish EspNow vs WiFi
    pairingSelect.value = Number(data.teleop_transport_mode) === 1 ? "teleop_wifi" : "teleop_espnow";
  }

  if (selectedRoleNode && document.activeElement !== selectedRoleNode && activeRole) {
    selectedRoleNode.value = activeRole === "follower" ? "calibration_follower" : "calibration_leader";
  }

  const modeNode = document.getElementById("calibrationModeCurrent");
  if (modeNode) {
    modeNode.textContent = activeRole ? `${activeRole} ${captureActive ? "range" : "center"}` : "idle";
    modeNode.dataset.captureActive = activeRole && captureActive ? "1" : "0";
    modeNode.dataset.activeRole = activeRole || "";
    modeNode.dataset.stage = activeRole ? (captureActive ? "range" : "center") : "idle";
  }

  const instructionNode = document.getElementById("calibrationInstruction");
  if (instructionNode) {
    instructionNode.textContent = calibrationInstruction(displayRole, activeRole === displayRole && captureActive);
    instructionNode.dataset.stage = activeRole ? (captureActive ? "range" : "center") : "idle";
  }

  const telemetryNode = document.getElementById("calibrationTelemetryCurrent");
  if (telemetryNode) {
    telemetryNode.textContent = telemetryText || "-";
  }

  renderCalibrationTable(
    document.getElementById("calibrationTableBody"),
    rows,
    calibrationProfile
  );
}

function setStickVector(id, axisX, axisY) {
  const node = document.getElementById(id);
  if (!node) {
    return;
  }

  const deadzoneInput = document.getElementById("xboxDeadzoneInput");
  const deadzonePct = Number(deadzoneInput?.value ?? 12);
  const deadzoneRatio = Number.isFinite(deadzonePct)
    ? Math.max(0, Math.min(0.95, deadzonePct / 100))
    : 0.12;

  const maxAbs = 32768;
  const rawNormalizedX = Math.max(-1, Math.min(1, Number(axisX || 0) / maxAbs));
  const rawNormalizedY = Math.max(-1, Math.min(1, Number(axisY || 0) / maxAbs));

  function applyDeadzone(value) {
    const magnitude = Math.abs(value);
    if (magnitude <= deadzoneRatio) {
      return 0;
    }

    const scale = (magnitude - deadzoneRatio) / (1 - deadzoneRatio);
    return Math.sign(value) * Math.max(0, Math.min(1, scale));
  }

  const normalizedX = applyDeadzone(rawNormalizedX);
  const normalizedY = applyDeadzone(rawNormalizedY);
  const travel = 76;
  const offsetX = normalizedX * travel;
  const offsetY = normalizedY * travel;

  node.style.transform = `translate(calc(-50% + ${offsetX}px), calc(-50% + ${offsetY}px))`;
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
  const dpadX = Number(data.xbox_dpad_x || 0);
  const dpadY = Number(data.xbox_dpad_y || 0);
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
  setStickVector("xboxLeftStickDot", leftX, leftY);
  setStickVector("xboxRightStickDot", rightX, rightY);

  const triggerNode = document.getElementById("xboxTriggers");
  if (triggerNode) {
    triggerNode.textContent = `${triggerLeft}, ${triggerRight}`;
  }

  setControlState("xboxBtnA", (buttonsMask & (1 << 0)) !== 0);
  setControlState("xboxBtnB", (buttonsMask & (1 << 1)) !== 0);
  setControlState("xboxBtnX", (buttonsMask & (1 << 2)) !== 0);
  setControlState("xboxBtnY", (buttonsMask & (1 << 3)) !== 0);
  setControlState("xboxBtnLB", (buttonsMask & (1 << 4)) !== 0);
  setControlState("xboxBtnRB", (buttonsMask & (1 << 5)) !== 0);
  setControlState("xboxDpadUp", dpadY < 0);
  setControlState("xboxDpadDown", dpadY > 0);
  setControlState("xboxDpadLeft", dpadX < 0);
  setControlState("xboxDpadRight", dpadX > 0);

  const runtimeDot = document.getElementById("xboxRuntimeDot");
  if (runtimeDot) {
    runtimeDot.classList.remove("state-green", "state-red", "state-amber");
    if (paired && encrypted && subscribed) {
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
  renderCalibrationPanel(data);

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
