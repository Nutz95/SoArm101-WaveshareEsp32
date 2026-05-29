export function initCalibrationView(commandWithStatus, saveTeleopConfig, refresh) {
  function errorText(rawError) {
    return rawError === "send_error" ? "leader stream disconnected" : (rawError || "send_error");
  }

  function selectedCalibrationValue() {
    const value = String(document.getElementById("calibrationModeSelect")?.value || "calibration_leader");
    return value === "calibration_follower" ? 3 : 2;
  }

  function captureIsActive() {
    const modeNode = document.getElementById("calibrationModeCurrent");
    return String(modeNode?.dataset?.captureActive || "0") === "1";
  }

  document.getElementById("servoScanBtn").addEventListener("click", async () => {
    await commandWithStatus("servo_scan", 0, "Servo scan (both) command sent.", "Servo scan failed (not connected).");
  });

  document.getElementById("servoScanLeaderBtn").addEventListener("click", async () => {
    await commandWithStatus("servo_scan_leader", 0, "Leader scan command sent.", "Leader scan failed.");
  });

  document.getElementById("servoScanFollowerBtn").addEventListener("click", async () => {
    await commandWithStatus("servo_scan_follower", 0, "Follower scan command sent.", "Follower scan failed.");
  });

  document.getElementById("calibrationActivateBtn").addEventListener("click", async () => {
    const sent = await commandWithStatus(
      "teleop_transport_set",
      selectedCalibrationValue(),
      "Calibration mode activated.",
      "Failed to activate calibration mode."
    );
    const statusNode = document.getElementById("calibrationStatus");
    if (!sent?.ok) {
      statusNode.textContent = `Calibration activation failed (${errorText(sent?.error)}).`;
      return;
    }

    await saveTeleopConfig({ calibration_required: true });
    statusNode.textContent = "Calibration mode active. Place all servos near 2048, then validate center. The firmware will verify the detected servos automatically.";
    await refresh();
  });

  document.getElementById("calibrationValidateABtn").addEventListener("click", async () => {
    const statusNode = document.getElementById("calibrationStatus");
    const rangeActive = captureIsActive();
    const commandValue = rangeActive ? 3 : 2;
    statusNode.textContent = rangeActive
      ? "Saving min/max ranges from the current sweep."
      : "Center validation running. The firmware is checking the detected servos sequentially and accepts positions close to 2048.";
    const sent = await commandWithStatus(
      "teleop_calibration_capture",
      commandValue,
      rangeActive ? "Calibration committed." : "Calibration center confirmed.",
      rangeActive ? "Failed to commit calibration." : "Failed to confirm calibration center."
    );
    if (!sent?.ok) {
      statusNode.textContent = rangeActive
        ? `Calibration commit failed (${errorText(sent?.error)}).`
        : `Center confirmation failed (${errorText(sent?.error)}).`;
      return;
    }

    if (!rangeActive) {
      statusNode.textContent = "Center confirmed. Move all joints together to explore min/max, then press Validate (A) again once for the whole arm.";
    } else {
      await saveTeleopConfig({ calibration_required: false, transport_mode: 0 });
      statusNode.textContent = "Calibration finished. Teleoperation ESP-NOW is active.";
    }

    await refresh();
  });

  document.getElementById("calibrationCancelBBtn").addEventListener("click", async () => {
    const statusNode = document.getElementById("calibrationStatus");
    const sent = await commandWithStatus(
      "teleop_calibration_capture",
      4,
      "Calibration canceled.",
      "Failed to cancel calibration."
    );
    if (!sent?.ok) {
      statusNode.textContent = `Calibration cancel failed (${errorText(sent?.error)}).`;
      return;
    }

    statusNode.textContent = "Calibration canceled. Stay in calibration mode or switch profile.";
    await refresh();
  });
}