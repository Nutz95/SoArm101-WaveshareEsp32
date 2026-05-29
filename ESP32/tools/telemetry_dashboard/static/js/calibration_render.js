const CALIBRATION_CENTER_TARGET = 2048;
const CALIBRATION_CENTER_OK_MARGIN = 128;
const CALIBRATION_LIMIT_WARNING_MARGIN = 200;
const SERVO_MIN_RAW = 0;
const SERVO_MAX_RAW = 4095;

function nvsMin(nvsProfile, servoId) {
  const idx = servoId - 1;
  return (nvsProfile && Array.isArray(nvsProfile.min) && idx >= 0 && idx < nvsProfile.min.length) ? nvsProfile.min[idx] : 0;
}

function nvsMax(nvsProfile, servoId) {
  const idx = servoId - 1;
  return (nvsProfile && Array.isArray(nvsProfile.max) && idx >= 0 && idx < nvsProfile.max.length) ? nvsProfile.max[idx] : 4095;
}

function calibrationClassName(state) {
  return state ? `calibration-${state}` : "";
}

function currentState(position, minValue, maxValue) {
  if (position <= minValue || position >= maxValue) {
    return "danger";
  }

  if (position <= (minValue + CALIBRATION_LIMIT_WARNING_MARGIN) || position >= (maxValue - CALIBRATION_LIMIT_WARNING_MARGIN)) {
    return "warn";
  }

  if (Math.abs(position - CALIBRATION_CENTER_TARGET) <= CALIBRATION_CENTER_OK_MARGIN) {
    return "ok";
  }

  return "";
}

function minState(minValue) {
  if (minValue <= SERVO_MIN_RAW) {
    return "danger";
  }
  if (minValue <= CALIBRATION_LIMIT_WARNING_MARGIN) {
    return "warn";
  }
  return "ok";
}

function maxState(maxValue) {
  if (maxValue >= SERVO_MAX_RAW) {
    return "danger";
  }
  if (maxValue >= (SERVO_MAX_RAW - CALIBRATION_LIMIT_WARNING_MARGIN)) {
    return "warn";
  }
  return "ok";
}

export function calibrationInstruction(role, captureActive) {
  if (!role) {
    return "Choose a calibration target, release the torque on that side, then start the workflow.";
  }

  if (!captureActive) {
    return `Center the ${role} servos by hand near 2048, then validate center. The firmware will verify each detected servo automatically before opening the min/max step.`;
  }

  return `Move the ${role} servos across both extremes, then validate with A to save and exit, or B to cancel.`;
}

export function renderCalibrationTable(tableBody, rows, nvsProfile) {
  if (!tableBody) {
    return;
  }

  tableBody.innerHTML = "";
  if (!rows.length) {
    const emptyRow = document.createElement("tr");
    emptyRow.innerHTML = '<td colspan="4">No live servo telemetry yet.</td>';
    tableBody.appendChild(emptyRow);
    return;
  }

  rows.forEach((row) => {
    const minValue = nvsMin(nvsProfile, row.id);
    const maxValue = nvsMax(nvsProfile, row.id);
    const atLimit = row.position <= minValue || row.position >= maxValue;
    const inverted = maxValue < minValue;
    const currentClass = inverted ? "calibration-danger" : calibrationClassName(currentState(row.position, minValue, maxValue));
    const minClass = inverted ? "calibration-danger" : calibrationClassName(minState(minValue));
    const maxClass = inverted ? "calibration-danger" : calibrationClassName(maxState(maxValue));
    const limitClass = (atLimit || inverted) ? "is-limit" : "";
    const tr = document.createElement("tr");
    tr.innerHTML = `
      <td>${row.id}</td>
      <td class="${row.position < 0 ? "is-negative" : ""} ${limitClass} ${currentClass}">${row.position}</td>
      <td class="${minValue < 0 ? "is-negative" : ""} ${minClass}">${minValue}</td>
      <td class="${maxValue < 0 ? "is-negative" : ""} ${inverted ? "is-limit" : ""} ${maxClass}">${maxValue}</td>
    `;
    tableBody.appendChild(tr);
  });
}