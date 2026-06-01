import { commandWithStatus } from "../api.js";



const profileToTransportValue = {

  calibration_leader: 2,

  calibration_follower: 3,

  teleop_espnow: 0,

  teleop_wifi: 1,

  passthrough: 4,

};



const profileLabels = {

  calibration_leader: "Calibration — leader",

  calibration_follower: "Calibration — follower",

  teleop_espnow: "Teleoperation — ESP-NOW",

  teleop_wifi: "Teleoperation — Wi-Fi UDP",

  passthrough: "Passthrough — USB to servo bus",

};



function profileFromSnapshot(data) {

  const profileValue = Number(data.controller_operation_profile ?? 2);

  if (profileValue === 0) {

    return "calibration_leader";

  }

  if (profileValue === 1) {

    return "calibration_follower";

  }

  if (profileValue === 4) {

    return "passthrough";

  }

  return Number(data.teleop_transport_mode) === 1 ? "teleop_wifi" : "teleop_espnow";

}



function formatPorts(ports) {

  if (!Array.isArray(ports) || ports.length === 0) {

    return "none detected";

  }

  return ports.map((entry) => `${entry.device}${entry.description ? ` (${entry.description})` : ""}`).join(", ");

}



async function refreshSerialBridgeState() {

  const statusNode = document.getElementById("serialBridgeStatus");

  const packetNode = document.getElementById("serialBridgePacketCount");

  const idleNode = document.getElementById("serialBridgeIdleReason");

  const portsNode = document.getElementById("serialBridgePorts");

  const followerNode = document.getElementById("serialBridgeFollowerCom");

  if (!statusNode || !packetNode) {

    return;

  }



  try {

    const response = await fetch("/api/serial-bridge/state");

    if (!response.ok) {

      statusNode.textContent = "Serial bridge API unavailable.";

      return;

    }

    const state = await response.json();

    if (followerNode && document.activeElement !== followerNode) {

      followerNode.value = String(state.follower_port || "COM8");

    }

    packetNode.textContent = String(state.packets_sent ?? state.packets_forwarded ?? 0);

    if (idleNode) {

      idleNode.textContent = String(state.idle_reason || "-");

    }

    if (portsNode) {

      portsNode.textContent = formatPorts(state.available_ports);

    }

    if (state.last_error) {

      statusNode.textContent = `Error: ${state.last_error}`;

      return;

    }

    if (state.running) {

      statusNode.textContent = `Running on ${state.follower_port || "?"}`;

      return;

    }

    statusNode.textContent = state.idle_reason ? `Stopped (${state.idle_reason})` : "Stopped";

  } catch (_error) {

    statusNode.textContent = "COM mirror state unavailable.";

  }

}



export function renderOperationModePanel(data) {

  const currentNode = document.getElementById("operationModeCurrent");

  const runtimeNode = document.getElementById("operationModeRuntime");

  const selectNode = document.getElementById("operationModeSelect");

  if (!currentNode || !selectNode) {

    return;

  }



  const profileKey = profileFromSnapshot(data);

  currentNode.textContent = profileLabels[profileKey] || profileKey;

  if (runtimeNode) {

    runtimeNode.textContent = String(data.mode_label || data.mode || "-");

  }

  if (document.activeElement !== selectNode) {

    selectNode.value = profileKey;

  }

  void refreshSerialBridgeState();

}



export function initModeView(refresh) {

  const applyBtn = document.getElementById("operationModeApplyBtn");

  const statusNode = document.getElementById("operationModeStatus");

  const selectNode = document.getElementById("operationModeSelect");

  const bridgeStartBtn = document.getElementById("serialBridgeStartBtn");

  const bridgeStopBtn = document.getElementById("serialBridgeStopBtn");

  const bridgeStatusNode = document.getElementById("serialBridgeStatus");

  const followerNode = document.getElementById("serialBridgeFollowerCom");

  if (!applyBtn || !statusNode || !selectNode) {

    return;

  }



  applyBtn.addEventListener("click", async () => {

    const profile = String(selectNode.value || "teleop_espnow");

    const transportValue = profileToTransportValue[profile];

    if (transportValue === undefined) {

      statusNode.textContent = "Unknown profile.";

      return;

    }



    const sent = await commandWithStatus(

      "teleop_transport_set",

      transportValue,

      `${profileLabels[profile] || profile} sent to leader.`,

      "Failed to apply operation profile on leader."

    );

    if (!sent?.ok) {

      statusNode.textContent = `Apply failed (${sent?.error || "send_error"}).`;

      return;

    }



    statusNode.textContent = `${profileLabels[profile] || profile} applied.`;

    await refresh();

  });



  if (bridgeStartBtn && bridgeStopBtn && bridgeStatusNode && followerNode) {

    bridgeStartBtn.addEventListener("click", async () => {

      try {

        const response = await fetch("/api/serial-bridge/start", {

          method: "POST",

          headers: { "Content-Type": "application/json" },

          body: JSON.stringify({

            mode: "telemetry",

            follower_port: String(followerNode.value || "COM8"),

          }),

        });

        const payload = await response.json();

        if (!response.ok || !payload.ok) {

          bridgeStatusNode.textContent = `Start failed: ${payload.error || response.status}`;

          await refreshSerialBridgeState();

          return;

        }

        bridgeStatusNode.textContent = "COM mirror started (see dashboard console for OPEN OK).";

        await refreshSerialBridgeState();

      } catch (_error) {

        bridgeStatusNode.textContent = "Start failed (network error).";

      }

    });



    bridgeStopBtn.addEventListener("click", async () => {

      try {

        const response = await fetch("/api/serial-bridge/stop", { method: "POST" });

        const payload = await response.json();

        if (!response.ok || !payload.ok) {

          bridgeStatusNode.textContent = `Stop failed (${payload.error || response.status}).`;

          return;

        }

        bridgeStatusNode.textContent = "COM mirror stopped.";

        await refreshSerialBridgeState();

      } catch (_error) {

        bridgeStatusNode.textContent = "Stop failed (network error).";

      }

    });

  }

}

