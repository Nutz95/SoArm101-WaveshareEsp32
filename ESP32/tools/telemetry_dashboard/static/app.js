import { fetchLatest, commandWithStatus } from "./js/api.js";
import { clampU8 } from "./js/servo_ui.js";
import { renderSnapshot } from "./js/dashboard_render.js";
import {
  hasPendingFollowerCommand,
  registerPendingCommand,
  syncPendingCommandStatus,
} from "./js/pending_command.js";

const REFRESH_INTERVAL_MS = 200;

async function refresh() {
  try {
    const data = await fetchLatest();
    renderSnapshot(data);
    syncPendingCommandStatus(data);
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
    await commandWithStatus("servo_scan", 0, "Servo scan (both) command sent.", "Servo scan failed (not connected).");
  });

  document.getElementById("servoScanLeaderBtn").addEventListener("click", async () => {
    await commandWithStatus("servo_scan_leader", 0, "Leader scan command sent.", "Leader scan failed.");
  });

  document.getElementById("servoScanFollowerBtn").addEventListener("click", async () => {
    await commandWithStatus("servo_scan_follower", 0, "Follower scan command sent.", "Follower scan failed.");
  });

  document.getElementById("servoDebugEnableBtn").addEventListener("click", async () => {
    const result = await commandWithStatus("servo_debug_enable", 0, "Servo debug/manual enabled.", "Enable debug failed.");
    registerPendingCommand(result, "leader", "leader debug enable");
  });

  document.getElementById("servoDebugDisableBtn").addEventListener("click", async () => {
    const result = await commandWithStatus("servo_debug_disable", 0, "Servo debug/manual disabled.", "Disable debug failed.");
    registerPendingCommand(result, "leader", "leader debug disable");
  });

  // Follower servo debug buttons (same commands, sent to leader which forwards via ESP-NOW)
  document.getElementById("followerServoDebugEnableBtn").addEventListener("click", async () => {
    if (hasPendingFollowerCommand()) {
      return;
    }
    const result = await commandWithStatus("servo_debug_enable_follower", 0, "Follower servo debug/manual enabled.", "Enable follower debug failed.");
    registerPendingCommand(result, "follower", "follower debug enable");
  });

  document.getElementById("followerServoDebugDisableBtn").addEventListener("click", async () => {
    if (hasPendingFollowerCommand()) {
      return;
    }
    const result = await commandWithStatus("servo_debug_disable_follower", 0, "Follower servo debug/manual disabled.", "Disable follower debug failed.");
    registerPendingCommand(result, "follower", "follower debug disable");
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
setInterval(refresh, REFRESH_INTERVAL_MS);
refresh();
