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

  return response.json();
}

export async function commandWithStatus(command, value, okText, failText) {
  const statusNode = document.getElementById("commandStatus");
  try {
    const result = await sendCommand(command, value);
    statusNode.textContent = result.ok ? okText : failText;
  } catch (_err) {
    statusNode.textContent = failText;
  }
}
