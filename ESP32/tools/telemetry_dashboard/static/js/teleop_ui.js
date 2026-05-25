function metricText(data, emptyText = "-") {
  if (!data) {
    return emptyText;
  }

  return `p${data.position} v${data.voltage} t${data.temperature} m${data.mode}`;
}

function syncControl(controlId, value, propertyName = "value") {
  const element = document.getElementById(controlId);
  if (!element) {
    return;
  }

  if (document.activeElement === element) {
    return;
  }

  element[propertyName] = value;
}

export function renderTeleopState(data) {
  const summary = data?.summary || {};
  const config = data?.config || {};
  const runtime = data?.runtime || {};
  const cards = Array.isArray(data?.cards) ? data.cards : [];

  document.getElementById("teleopMatchedCount").textContent = `${summary.matched || 0}`;
  document.getElementById("teleopMirrorableCount").textContent = `${summary.mirrorable || 0}`;
  document.getElementById("teleopLeaderDetected").textContent = `${summary.leader_detected || 0}`;
  document.getElementById("teleopFollowerDetected").textContent = `${summary.follower_detected || 0}`;
  const continuousNode = document.getElementById("teleopContinuousState");
  const continuousEnabled = !!runtime.continuous_enabled;
  const continuousServoId = Number(runtime.continuous_servo_id || 0);
  continuousNode.textContent = continuousEnabled
    ? (continuousServoId === 0 ? "all matched" : `ID ${continuousServoId}`)
    : "off";
  continuousNode.classList.toggle("state-on", continuousEnabled);
  syncControl("teleopEnabledInput", !!config.enabled, "checked");
  syncControl("teleopSameIdInput", !!config.same_id_mapping, "checked");
  syncControl("teleopCalibrationInput", !!config.calibration_required, "checked");
  syncControl("teleopSpeedInput", `${config.speed_pct ?? 35}`);

  const container = document.getElementById("teleopServoCards");
  container.innerHTML = "";
  if (cards.length === 0) {
    const empty = document.createElement("div");
    empty.className = "card teleop-card teleop-empty";
    empty.textContent = "No detected servos yet. Run a scan to populate teleoperation cards.";
    container.appendChild(empty);
    return;
  }

  cards.forEach((card) => {
    const article = document.createElement("article");
    article.className = "card teleop-card";
    if (card.can_mirror) {
      article.classList.add("teleop-ready");
    }
    if (card.active_continuous) {
      article.classList.add("teleop-active");
    }

    const stateLabel = card.mapped ? (card.can_mirror ? "ready" : "mapped") : "unmatched";
    const deltaText = Number.isFinite(card.position_delta) ? `${card.position_delta}` : "-";

    const continuousAction = card.active_continuous ? "stop" : "start";
    const continuousText = card.active_continuous ? "Stop Mirror Loop" : "Mirror This Servo (Loop)";

    article.innerHTML = `
      <div class="teleop-card-header">
        <strong>ID ${card.servo_id}</strong>
        <span class="teleop-pill">${stateLabel}</span>
      </div>
      <div class="teleop-card-grid">
        <div>
          <span>Leader</span>
          <strong>${card.leader_present ? "present" : "missing"}</strong>
          <p>${metricText(card.leader)}</p>
        </div>
        <div>
          <span>Follower</span>
          <strong>${card.follower_present ? "present" : "missing"}</strong>
          <p>${metricText(card.follower)}</p>
        </div>
      </div>
      <div class="teleop-card-footer">
        <span>Delta ${deltaText}</span>
        <button type="button" data-teleop-servo-id="${card.servo_id}" data-teleop-continuous-action="${continuousAction}" ${card.can_mirror ? "" : "disabled"}>${continuousText}</button>
      </div>
    `;
    container.appendChild(article);
  });
}