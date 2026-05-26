import { clampU8 } from "../servo_ui.js";

export function initTeleopView(commandWithStatus, saveTeleopConfig, triggerTeleopMirror, refresh) {
  const controlIds = [
    "teleopEnabledInput",
    "teleopSameIdInput",
    "teleopCalibrationInput",
    "teleopSpeedInput",
    "teleopTransportInput",
  ];

  function markControlsDirty(isDirty) {
    controlIds.forEach((id) => {
      const node = document.getElementById(id);
      if (!node) {
        return;
      }
      node.dataset.userDirty = isDirty ? "1" : "0";
    });
  }

  function readConfigFromUi() {
    return {
      enabled: document.getElementById("teleopEnabledInput").checked,
      same_id_mapping: document.getElementById("teleopSameIdInput").checked,
      calibration_required: document.getElementById("teleopCalibrationInput").checked,
      speed_pct: clampU8(Number(document.getElementById("teleopSpeedInput").value)),
      transport_mode: Number(document.getElementById("teleopTransportInput").value) === 1 ? 1 : 0,
    };
  }

  function buildContinuousPackedValue(enable, servoId, speedPct) {
    const enableBit = enable ? 1 : 0;
    return (enableBit | ((servoId & 0xFF) << 8) | ((speedPct & 0xFF) << 16)) >>> 0;
  }

  function readContinuousRuntime() {
    const stateNode = document.getElementById("teleopContinuousState");
    const enabled = stateNode?.dataset?.enabled === "1";
    const servoId = clampU8(Number(stateNode?.dataset?.servoId || 0));
    return { enabled, servoId };
  }

  async function applyConfigFromControls() {
    markControlsDirty(true);
    const config = readConfigFromUi();
    const statusNode = document.getElementById("teleopStatus");
    try {
      const result = await saveTeleopConfig(config);
      if (!result?.ok) {
        statusNode.textContent = "Teleoperation config save failed.";
        return false;
      }

      statusNode.textContent = "Teleoperation config applied.";
      await refresh();
      return true;
    } finally {
      markControlsDirty(false);
    }
  }

  document.getElementById("teleopSaveConfigBtn").addEventListener("click", async () => {
    await applyConfigFromControls();
  });

  document.getElementById("teleopEnabledInput").addEventListener("change", async () => {
    await applyConfigFromControls();
  });

  document.getElementById("teleopSameIdInput").addEventListener("change", async () => {
    await applyConfigFromControls();
  });

  document.getElementById("teleopCalibrationInput").addEventListener("change", async () => {
    await applyConfigFromControls();
  });

  document.getElementById("teleopSpeedInput").addEventListener("change", async () => {
    const applied = await applyConfigFromControls();
    if (!applied) {
      return;
    }

    const currentSpeed = clampU8(Number(document.getElementById("teleopSpeedInput").value));
    const continuous = readContinuousRuntime();
    const packed = buildContinuousPackedValue(continuous.enabled, continuous.servoId, currentSpeed);
    await commandWithStatus(
      "teleop_continuous_set",
      packed,
      "Continuous speed updated.",
      "Failed to update continuous speed."
    );
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
    const speedPct = clampU8(Number(document.getElementById("teleopSpeedInput").value));
    const packed = buildContinuousPackedValue(true, 0, speedPct);
    const result = await commandWithStatus(
      "teleop_continuous_set",
      packed,
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
    const speedPct = clampU8(Number(document.getElementById("teleopSpeedInput").value));
    const packed = buildContinuousPackedValue(false, 0, speedPct);
    const result = await commandWithStatus(
      "teleop_continuous_set",
      packed,
      "Continuous mirror stopped.",
      "Failed to stop continuous mirror."
    );
    if (result?.ok) {
      await refresh();
    } else {
      statusNode.textContent = `Continuous mirror stop failed (${result?.error || "send_error"}).`;
    }
  });

  async function applyTransportModeFromControl() {
    const statusNode = document.getElementById("teleopStatus");
    const transportMode = Number(document.getElementById("teleopTransportInput").value) === 1 ? 1 : 0;
    markControlsDirty(true);
    try {
      const result = await commandWithStatus(
        "teleop_transport_set",
        transportMode,
        transportMode === 1 ? "Teleop transport set to Wi-Fi UDP." : "Teleop transport set to ESP-NOW.",
        "Failed to set teleop transport."
      );
      if (!result?.ok) {
        statusNode.textContent = `Teleop transport update failed (${result?.error || "send_error"}).`;
        return;
      }

      await saveTeleopConfig(readConfigFromUi());
      await refresh();
    } finally {
      markControlsDirty(false);
    }
  }

  document.getElementById("teleopApplyTransportBtn").addEventListener("click", async () => {
    await applyTransportModeFromControl();
  });

  document.getElementById("teleopTransportInput").addEventListener("change", async () => {
    await applyTransportModeFromControl();
  });

  document.getElementById("teleopServoCards").addEventListener("click", async (event) => {
    const button = event.target.closest("button[data-teleop-servo-id]");
    if (!button) {
      return;
    }

    const servoId = clampU8(Number(button.dataset.teleopServoId));
    const action = String(button.dataset.teleopContinuousAction || "start").toLowerCase();
    const speedPct = clampU8(Number(document.getElementById("teleopSpeedInput").value));
    const packed = buildContinuousPackedValue(action !== "stop", action === "stop" ? 0 : servoId, speedPct);
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
