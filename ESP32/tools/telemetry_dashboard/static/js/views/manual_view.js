import { clampU8 } from "../servo_ui.js";

const SERVO_CENTER_POSITION = 2048;

function buildServoMovePackedValue(servoId, position, speedPercent) {
  return ((servoId & 0xFF) | ((position & 0xFFFF) << 8) | ((speedPercent & 0xFF) << 24)) >>> 0;
}

export function initManualView(commandWithStatus, hasPendingFollowerCommand, registerPendingCommand) {
  document.getElementById("leaderServoDebugToggleBtn").addEventListener("click", async (event) => {
    const toggleButton = event.currentTarget;
    const isEnabled = String(toggleButton?.dataset?.enabled || "0") === "1";
    const command = isEnabled ? "servo_debug_disable" : "servo_debug_enable";
    const okText = isEnabled ? "Servo debug/manual disabled." : "Servo debug/manual enabled.";
    const failText = isEnabled ? "Disable debug failed." : "Enable debug failed.";

    const result = await commandWithStatus(command, 0, okText, failText);
    registerPendingCommand(result, "leader", isEnabled ? "leader debug disable" : "leader debug enable");
  });

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
    const packed = buildServoMovePackedValue(id, position, speedPct);
    await commandWithStatus("servo_move", packed >>> 0, "Servo move command sent.", "Servo move failed.");
  });

  document.getElementById("servoCenterBtn").addEventListener("click", async () => {
    const statusNode = document.getElementById("manualStatus");
    const id = clampU8(Number(document.getElementById("servoIdInput").value));
    const speedPercent = clampU8(Number(document.getElementById("servoSpeedInput").value));
    document.getElementById("servoPositionInput").value = `${SERVO_CENTER_POSITION}`;

    const packed = buildServoMovePackedValue(id, SERVO_CENTER_POSITION, speedPercent);
    const result = await commandWithStatus(
      "servo_move",
      packed,
      `Center command sent for servo ID ${id}.`,
      `Center command failed for servo ID ${id}.`
    );

    if (result?.ok) {
      if (statusNode) {
        statusNode.textContent = `Servo ID ${id} moved to center (${SERVO_CENTER_POSITION}).`;
      }
    } else {
      if (statusNode) {
        statusNode.textContent = `Center failed on servo ID ${id} (${result?.error || "send_error"}).`;
      }
    }
  });

  document.getElementById("servoSetIdBtn").addEventListener("click", async () => {
    const oldId = clampU8(Number(document.getElementById("servoOldIdInput").value));
    const newId = clampU8(Number(document.getElementById("servoNewIdInput").value));
    const packed = (oldId & 0xFF) | ((newId & 0xFF) << 8);
    await commandWithStatus("servo_set_id", packed >>> 0, "Servo ID update sent.", "Servo ID update failed.");
  });

  document.getElementById("servoSetModeBtn").addEventListener("click", async () => {
    const id = clampU8(Number(document.getElementById("servoModeIdInput").value));
    const mode = clampU8(Number(document.getElementById("servoModeInput").value));
    const packed = (id & 0xFF) | ((mode & 0xFF) << 8);
    await commandWithStatus("servo_set_mode", packed >>> 0, "Servo mode update sent.", "Servo mode update failed.");
  });
}
