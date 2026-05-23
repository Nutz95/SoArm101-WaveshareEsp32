export function modeLabel(v) {
  switch (v) {
    case 0: return "Idle";
    case 1: return "CalibrationLeader";
    case 2: return "CalibrationFollower";
    case 3: return "Teleoperation";
    default: return "Unknown";
  }
}

export function stateLabel(v) {
  switch (v) {
    case 0: return "PairingOrUnpaired";
    case 1: return "Paired";
    case 2: return "WaitingCalibration";
    case 3: return "WaitingEspNow";
    case 4: return "Ready";
    case 5: return "ServoFault";
    default: return "Unknown";
  }
}

export function uptimeLabel(ms) {
  const totalSeconds = Math.floor(ms / 1000);
  const hours = Math.floor(totalSeconds / 3600);
  const minutes = Math.floor((totalSeconds % 3600) / 60);
  const seconds = totalSeconds % 60;
  return `${String(hours).padStart(2, "0")}:${String(minutes).padStart(2, "0")}:${String(seconds).padStart(2, "0")}`;
}

export function boolLabel(v) {
  return v ? "on" : "off";
}
