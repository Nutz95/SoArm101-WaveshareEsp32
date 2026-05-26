export function initPairingView(commandWithStatus, saveTeleopConfig, refresh) {
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

  document.getElementById("xboxPairingGuideBtn").addEventListener("click", () => {
    const statusNode = document.getElementById("xboxPairingStatus");
    statusNode.textContent = "Put the Xbox controller in pairing mode, then pair from the host Bluetooth settings.";
  });

  document.getElementById("pairingApplyModeBtn").addEventListener("click", async () => {
    const statusNode = document.getElementById("xboxPairingStatus");
    const profile = String(document.getElementById("pairingModeSelect").value || "teleop_espnow");

    if (profile === "calibration") {
      await saveTeleopConfig({ calibration_required: true });
      statusNode.textContent = "Calibration profile applied.";
      await refresh();
      return;
    }

    const transportMode = profile === "teleop_wifi" ? 1 : 0;
    const sent = await commandWithStatus(
      "teleop_transport_set",
      transportMode,
      transportMode === 1 ? "Wi-Fi teleoperation profile sent." : "ESP-NOW teleoperation profile sent.",
      "Failed to switch teleoperation profile."
    );
    if (!sent?.ok) {
      statusNode.textContent = `Mode switch failed (${sent?.error || "send_error"}).`;
      return;
    }

    await saveTeleopConfig({ calibration_required: false, transport_mode: transportMode });
    statusNode.textContent = transportMode === 1 ? "Teleoperation Wi-Fi profile applied." : "Teleoperation ESP-NOW profile applied.";
    await refresh();
  });
}
