export async function fetchLatest() {
  const response = await fetch("/api/latest", { cache: "no-store" });
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
