export function clampU8(value) {
  if (!Number.isFinite(value)) {
    return 0;
  }
  return Math.max(0, Math.min(255, value | 0));
}

function parseIds(raw) {
  if (!raw || raw === "-") {
    return [];
  }

  return raw
    .split(/[ ,;|]+/)
    .map((token) => token.trim())
    .filter((token) => token.length > 0);
}

export function renderServoChips(containerId, rawIds) {
  const container = document.getElementById(containerId);
  if (!container) {
    return;
  }

  const ids = parseIds(rawIds);
  container.innerHTML = "";

  if (ids.length === 0) {
    const empty = document.createElement("span");
    empty.className = "chip empty";
    empty.textContent = "none";
    container.appendChild(empty);
    return;
  }

  ids.forEach((id) => {
    const chip = document.createElement("span");
    chip.className = "chip";
    chip.textContent = `ID ${id}`;
    container.appendChild(chip);
  });
}
