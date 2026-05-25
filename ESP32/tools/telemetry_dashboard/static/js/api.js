export async function fetchLatest() {
  const response = await fetch("/api/latest", { cache: "no-store" });
  return response.json();
}

export async function fetchTeleopState() {
  const response = await fetch("/api/teleop/state", { cache: "no-store" });
  return response.json();
}

export async function sendCommand(command, value = 0) {
  const response = await fetch("/api/command", {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
    },
    body: JSON.stringify({ command, value }),
  });

  let payload = {};
  try {
    payload = await response.json();
  } catch (_err) {
    payload = { ok: false, error: "invalid_response" };
  }

  if (!response.ok) {
    return {
      ok: false,
      error: payload.error || `http_${response.status}`,
      command,
    };
  }

  return payload;
}

export async function commandWithStatus(command, value, okText, failText) {
  const statusNode = document.getElementById("commandStatus");
  try {
    const result = await sendCommand(command, value);
    if (result.ok) {
      const requestSuffix = result.request_id ? ` (req ${result.request_id})` : "";
      statusNode.textContent = `${okText}${requestSuffix}`;
      return result;
    }

    const errorText = result.error ? ` (${result.error})` : "";
    statusNode.textContent = `${failText}${errorText}`;
    return result;
  } catch (_err) {
    statusNode.textContent = failText;
    return { ok: false, error: "network_error", command };
  }
}

export async function saveTeleopConfig(config) {
  const response = await fetch("/api/teleop/config", {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
    },
    body: JSON.stringify(config),
  });
  return response.json();
}

export async function triggerTeleopMirror(servoId = null) {
  const payload = {};
  if (servoId !== null) {
    payload.servo_id = servoId;
  }

  const response = await fetch("/api/teleop/mirror", {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
    },
    body: JSON.stringify(payload),
  });
  return response.json();
}
