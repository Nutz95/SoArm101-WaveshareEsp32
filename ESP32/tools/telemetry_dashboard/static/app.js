import { fetchLatest, fetchTeleopState, commandWithStatus, saveTeleopConfig, triggerTeleopMirror } from "./js/api.js";
import { clampU8 } from "./js/servo_ui.js";
import { renderSnapshot } from "./js/dashboard_render.js";
import { renderTeleopState } from "./js/teleop_ui.js";
import {
  hasPendingFollowerCommand,
  registerPendingCommand,
  syncPendingCommandStatus,
} from "./js/pending_command.js";

const REFRESH_INTERVAL_MS = 200;

async function refresh() {
  try {
    const [data, teleopState] = await Promise.all([fetchLatest(), fetchTeleopState()]);
    renderSnapshot(data);
    renderTeleopState(teleopState);
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

  document.getElementById("teleopSaveConfigBtn").addEventListener("click", async () => {
    const config = {
      enabled: document.getElementById("teleopEnabledInput").checked,
      same_id_mapping: document.getElementById("teleopSameIdInput").checked,
      calibration_required: document.getElementById("teleopCalibrationInput").checked,
      speed_pct: clampU8(Number(document.getElementById("teleopSpeedInput").value)),
    };
    const result = await saveTeleopConfig(config);
    const statusNode = document.getElementById("teleopStatus");
    statusNode.textContent = result?.ok ? "Teleoperation config saved." : "Teleoperation config save failed.";
    await refresh();
  });

  document.getElementById("teleopMirrorAllBtn").addEventListener("click", async () => {
    const statusNode = document.getElementById("teleopStatus");
    const result = await triggerTeleopMirror();
    if (result?.ok) {
      statusNode.textContent = `Mirror batch sent for ${result.sent_count}/${result.requested_count} servo(s).`;
      return;
    }
    statusNode.textContent = `Mirror batch failed (${result?.error || "send_error"}).`;
  });

  document.getElementById("teleopServoCards").addEventListener("click", async (event) => {
    const button = event.target.closest("button[data-teleop-servo-id]");
    if (!button) {
      return;
    }

    const servoId = clampU8(Number(button.dataset.teleopServoId));
    const result = await triggerTeleopMirror(servoId);
    const statusNode = document.getElementById("teleopStatus");
    if (result?.ok && Array.isArray(result.request_ids) && result.request_ids.length === 1) {
      statusNode.textContent = `Mirror servo ID ${servoId} sent.`;
      registerPendingCommand({ ok: true, request_id: result.request_ids[0] }, "follower", `teleop mirror ID ${servoId}`);
      return;
    }
    statusNode.textContent = `Mirror servo ID ${servoId} failed (${result?.error || "send_error"}).`;
  });
}

setupButtons();
setInterval(refresh, REFRESH_INTERVAL_MS);
refresh();
