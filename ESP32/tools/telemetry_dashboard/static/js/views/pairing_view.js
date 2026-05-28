function clampPercent(value, fallback) {
  const numeric = Number(value);
  if (!Number.isFinite(numeric)) {
    return fallback;
  }
  return Math.max(0, Math.min(100, Math.round(numeric)));
}

function modeCycleButtonValue(key) {
  const normalized = String(key || "").trim().toLowerCase();
  const modeCycleByKey = {
    view: 1,
    menu: 2,
    share: 3,
    left_stick: 4,
    right_stick: 5,
    none: 0,
  };

  if (Object.prototype.hasOwnProperty.call(modeCycleByKey, normalized)) {
    return modeCycleByKey[normalized];
  }

  return 0;
}

function syncXboxConfigControls(config = {}) {
  const enabledInput = document.getElementById("xboxControllerEnabledInput");
  const nameInput = document.getElementById("xboxPreferredControllerInput");
  const autoReconnectInput = document.getElementById("xboxAutoReconnectInput");
  const deadzoneInput = document.getElementById("xboxDeadzoneInput");
  const triggerThresholdInput = document.getElementById("xboxTriggerThresholdInput");
  const invertLeftYInput = document.getElementById("xboxInvertLeftYInput");
  const invertRightYInput = document.getElementById("xboxInvertRightYInput");
  const modeCycleSelect = document.getElementById("xboxModeCycleButtonSelect");
  const userActionSelect = document.getElementById("xboxUserActionButtonSelect");

  if (enabledInput) {
    enabledInput.checked = !!config.enabled;
  }
  if (nameInput && document.activeElement !== nameInput) {
    nameInput.value = String(config.preferred_controller_name || "Xbox Wireless Controller");
  }
  if (autoReconnectInput) {
    autoReconnectInput.checked = !!config.auto_reconnect;
  }
  if (deadzoneInput && document.activeElement !== deadzoneInput) {
    deadzoneInput.value = `${config.deadzone_pct ?? 12}`;
  }
  if (triggerThresholdInput && document.activeElement !== triggerThresholdInput) {
    triggerThresholdInput.value = `${config.trigger_threshold_pct ?? 35}`;
  }
  if (invertLeftYInput) {
    invertLeftYInput.checked = !!config.invert_left_y;
  }
  if (invertRightYInput) {
    invertRightYInput.checked = !!config.invert_right_y;
  }
  if (modeCycleSelect && document.activeElement !== modeCycleSelect) {
    modeCycleSelect.value = String(config.mode_cycle_button || "view");
  }
  if (userActionSelect && document.activeElement !== userActionSelect) {
    userActionSelect.value = String(config.user_action_button || "share");
  }

  const configStatus = document.getElementById("xboxConfigStatus");
  if (configStatus) {
    configStatus.textContent = config.enabled ? "ready" : "disabled";
  }

  const preferredCurrent = document.getElementById("xboxPreferredControllerCurrent");
  if (preferredCurrent) {
    preferredCurrent.textContent = String(config.preferred_controller_name || "Xbox Wireless Controller");
  }
}

export function initPairingView(commandWithStatus, saveTeleopConfig, fetchControllerConfig, saveControllerConfig, refresh) {
  async function loadControllerConfig() {
    try {
      const payload = await fetchControllerConfig();
      syncXboxConfigControls(payload?.config || {});
    } catch (_err) {
    }
  }

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
    statusNode.textContent = "Put the Xbox controller in pairing mode, pair it from the host Bluetooth settings, then keep the preferred name below aligned with the actual device name.";
  });

  document.getElementById("xboxSaveConfigBtn").addEventListener("click", async () => {
    const statusNode = document.getElementById("xboxPairingStatus");
    const payload = {
      enabled: document.getElementById("xboxControllerEnabledInput").checked,
      preferred_controller_name: String(document.getElementById("xboxPreferredControllerInput").value || "").trim(),
      auto_reconnect: document.getElementById("xboxAutoReconnectInput").checked,
      deadzone_pct: clampPercent(document.getElementById("xboxDeadzoneInput").value, 12),
      trigger_threshold_pct: clampPercent(document.getElementById("xboxTriggerThresholdInput").value, 35),
      invert_left_y: document.getElementById("xboxInvertLeftYInput").checked,
      invert_right_y: document.getElementById("xboxInvertRightYInput").checked,
      mode_cycle_button: String(document.getElementById("xboxModeCycleButtonSelect").value || "view"),
      user_action_button: String(document.getElementById("xboxUserActionButtonSelect").value || "share"),
    };

    const result = await saveControllerConfig(payload);
    if (!result?.ok) {
      statusNode.textContent = "Xbox config save failed.";
      return;
    }

    const modeCycleValue = modeCycleButtonValue(payload.mode_cycle_button);
    const modeCycleSent = await commandWithStatus(
      "xbox_mode_cycle_button_set",
      modeCycleValue,
      "Xbox mode-cycle button sent to leader firmware.",
      "Failed to send Xbox mode-cycle button to leader firmware."
    );
    if (!modeCycleSent?.ok) {
      statusNode.textContent = `Xbox config saved, but firmware update failed (${modeCycleSent?.error || "send_error"}).`;
      return;
    }

    syncXboxConfigControls(result.config || payload);
    statusNode.textContent = "Xbox config saved and mode-cycle button applied on leader.";
  });

  document.getElementById("pairingApplyModeBtn").addEventListener("click", async () => {
    const statusNode = document.getElementById("xboxPairingStatus");
    const profile = String(document.getElementById("pairingModeSelect").value || "teleop_espnow");

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

  loadControllerConfig();
}
