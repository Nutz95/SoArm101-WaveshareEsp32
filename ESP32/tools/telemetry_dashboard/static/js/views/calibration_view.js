export function initCalibrationView(commandWithStatus, saveTeleopConfig, refresh) {
  let centerValidated = false;

  function selectedCalibrationValue() {
    const value = String(document.getElementById("calibrationModeSelect")?.value || "calibration_leader");
    return value === "calibration_follower" ? 3 : 2;
  }

  document.getElementById("calibrationActivateBtn").addEventListener("click", async () => {
    const sent = await commandWithStatus(
      "teleop_transport_set",
      selectedCalibrationValue(),
      "Calibration mode activated.",
      "Failed to activate calibration mode."
    );
    const statusNode = document.getElementById("calibrationStatus");
    if (!sent?.ok) {
      statusNode.textContent = `Calibration activation failed (${sent?.error || "send_error"}).`;
      return;
    }

    await saveTeleopConfig({ calibration_required: true });
    centerValidated = false;
    statusNode.textContent = "Calibration mode active. Center servos first, then confirm center.";
    await refresh();
  });

  document.getElementById("calibrationValidateABtn").addEventListener("click", async () => {
    const statusNode = document.getElementById("calibrationStatus");
    const commandValue = centerValidated ? 3 : 2;
    const sent = await commandWithStatus(
      "teleop_calibration_capture",
      commandValue,
      centerValidated ? "Calibration committed." : "Calibration center confirmed.",
      centerValidated ? "Failed to commit calibration." : "Failed to confirm calibration center."
    );
    if (!sent?.ok) {
      statusNode.textContent = centerValidated
        ? `Calibration commit failed (${sent?.error || "send_error"}).`
        : `Center confirmation failed (${sent?.error || "send_error"}).`;
      return;
    }

    if (!centerValidated) {
      centerValidated = true;
      statusNode.textContent = "Center confirmed. Move all joints to explore min/max, then press Validate (A) again.";
    } else {
      centerValidated = false;
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
      statusNode.textContent = `Calibration cancel failed (${sent?.error || "send_error"}).`;
      return;
    }

    centerValidated = false;
    statusNode.textContent = "Calibration canceled. Stay in calibration mode or switch profile.";
    await refresh();
  });
}