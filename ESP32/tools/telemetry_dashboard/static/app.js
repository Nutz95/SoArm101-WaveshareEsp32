import { fetchLatest, commandWithStatus } from "./js/api.js";
import { modeLabel, stateLabel, uptimeLabel, boolLabel } from "./js/formatters.js";
import { clampU8, renderServoChips } from "./js/servo_ui.js";

async function refresh() {
  try {
    const data = await fetchLatest();

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

    renderServoChips("leaderServoIds", data.leader_servo_ids);
    renderServoChips("followerServoIds", data.follower_servo_ids);
  } catch (_err) {
  }
}

function setupButtons() {
  document.getElementById("resetPairingBtn").addEventListener("click", async () => {
    await commandWithStatus(
      "reset_pairing",
      0,
      "Pairing reset command sent, relaunch requested.",
      "Pairing reset failed (not connected)."
    );
  });

  document.getElementById("servoScanBtn").addEventListener("click", async () => {
    await commandWithStatus("servo_scan", 0, "Servo scan command sent.", "Servo scan failed (not connected).");
  });

  document.getElementById("servoDebugEnableBtn").addEventListener("click", async () => {
    await commandWithStatus("servo_debug_enable", 0, "Servo debug/manual enabled.", "Enable debug failed.");
  });

  document.getElementById("servoDebugDisableBtn").addEventListener("click", async () => {
    await commandWithStatus("servo_debug_disable", 0, "Servo debug/manual disabled.", "Disable debug failed.");
  });

  document.getElementById("servoMoveBtn").addEventListener("click", async () => {
    const id = clampU8(Number(document.getElementById("servoIdInput").value));
    const position = Number(document.getElementById("servoPositionInput").value) | 0;
    const speedPct = clampU8(Number(document.getElementById("servoSpeedInput").value));
    const packed = (id & 0xFF) | ((position & 0xFFFF) << 8) | ((speedPct & 0xFF) << 24);
    await commandWithStatus(
      "servo_move",
      packed >>> 0,
      "Servo move command sent.",
      "Servo move failed."
    );
  });

  document.getElementById("servoSetIdBtn").addEventListener("click", async () => {
    const oldId = clampU8(Number(document.getElementById("servoOldIdInput").value));
    const newId = clampU8(Number(document.getElementById("servoNewIdInput").value));
    const packed = (oldId & 0xFF) | ((newId & 0xFF) << 8);
    await commandWithStatus(
      "servo_set_id",
      packed >>> 0,
      "Servo ID update sent.",
      "Servo ID update failed."
    );
  });

  document.getElementById("servoSetModeBtn").addEventListener("click", async () => {
    const id = clampU8(Number(document.getElementById("servoModeIdInput").value));
    const mode = clampU8(Number(document.getElementById("servoModeInput").value));
    const packed = (id & 0xFF) | ((mode & 0xFF) << 8);
    await commandWithStatus(
      "servo_set_mode",
      packed >>> 0,
      "Servo mode update sent.",
      "Servo mode update failed."
    );
  });
}

setupButtons();
setInterval(refresh, 200);
refresh();
