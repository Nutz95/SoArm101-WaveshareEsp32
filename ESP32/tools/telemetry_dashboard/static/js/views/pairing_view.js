export function initPairingView(commandWithStatus) {
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
}
