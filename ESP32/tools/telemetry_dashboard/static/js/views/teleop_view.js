import { clampU8 } from "../servo_ui.js";

export function initTeleopView(commandWithStatus, saveTeleopConfig, triggerTeleopMirror, refresh) {
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

  document.getElementById("teleopContinuousAllBtn").addEventListener("click", async () => {
    const statusNode = document.getElementById("teleopStatus");
    const result = await commandWithStatus(
      "teleop_continuous_set",
      1,
      "Continuous mirror started for all matched servos.",
      "Failed to start continuous mirror."
    );
    if (result?.ok) {
      await refresh();
    } else {
      statusNode.textContent = `Continuous mirror start failed (${result?.error || "send_error"}).`;
    }
  });

  document.getElementById("teleopContinuousStopBtn").addEventListener("click", async () => {
    const statusNode = document.getElementById("teleopStatus");
    const result = await commandWithStatus(
      "teleop_continuous_set",
      0,
      "Continuous mirror stopped.",
      "Failed to stop continuous mirror."
    );
    if (result?.ok) {
      await refresh();
    } else {
      statusNode.textContent = `Continuous mirror stop failed (${result?.error || "send_error"}).`;
    }
  });

  document.getElementById("teleopServoCards").addEventListener("click", async (event) => {
    const button = event.target.closest("button[data-teleop-servo-id]");
    if (!button) {
      return;
    }

    const servoId = clampU8(Number(button.dataset.teleopServoId));
    const action = String(button.dataset.teleopContinuousAction || "start").toLowerCase();
    const packed = action === "stop" ? 0 : ((1 & 0x01) | ((servoId & 0xFF) << 8));
    const result = await commandWithStatus(
      "teleop_continuous_set",
      packed >>> 0,
      action === "stop" ? `Continuous mirror stopped (ID ${servoId}).` : `Continuous mirror started for ID ${servoId}.`,
      action === "stop" ? `Failed to stop ID ${servoId}.` : `Failed to start ID ${servoId}.`
    );
    const statusNode = document.getElementById("teleopStatus");
    if (result?.ok) {
      statusNode.textContent = action === "stop" ? "Continuous mirror stopped." : `Continuous mirror active on servo ID ${servoId}.`;
      await refresh();
      return;
    }
    statusNode.textContent = `Continuous mirror update failed (${result?.error || "send_error"}).`;
  });
}
