import { renderLagMetrics } from "./lag_metrics.js";

const CHART_MAX_POINTS = 160;
const CHART_WINDOW_MS = 20000;
const CHART_MARGIN = 28;
const CHART_COLORS = ["#5eead4", "#f59e0b", "#60a5fa", "#ef4444", "#34d399", "#f472b6", "#22d3ee", "#a3e635"];
const historyByServoId = new Map();

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

  if (element.dataset.userDirty === "1") {
    return;
  }

  if (document.activeElement === element) {
    return;
  }

  element[propertyName] = value;
}

function servoColor(servoId) {
  const index = Math.abs(Number(servoId) || 0) % CHART_COLORS.length;
  return CHART_COLORS[index];
}

function appendHistory(cards) {
  const now = Date.now();

  cards.forEach((card) => {
    const servoId = Number(card?.servo_id || 0);
    if (!servoId) {
      return;
    }

    const leaderPosition = Number(card?.leader?.position);
    const followerPosition = Number(card?.follower?.position);
    const hasLeader = Number.isFinite(leaderPosition);
    const hasFollower = Number.isFinite(followerPosition);
    if (!hasLeader && !hasFollower) {
      return;
    }

    const history = historyByServoId.get(servoId) || [];
    history.push({
      t: now,
      l: hasLeader ? leaderPosition : null,
      f: hasFollower ? followerPosition : null,
    });

    if (history.length > CHART_MAX_POINTS) {
      history.splice(0, history.length - CHART_MAX_POINTS);
    }

    const minTs = now - CHART_WINDOW_MS;
    while (history.length > 0 && history[0].t < minTs) {
      history.shift();
    }

    historyByServoId.set(servoId, history);
  });
}

function collectChartDomain(servoIds) {
  let minY = Number.POSITIVE_INFINITY;
  let maxY = Number.NEGATIVE_INFINITY;

  servoIds.forEach((servoId) => {
    const history = historyByServoId.get(servoId) || [];
    history.forEach((sample) => {
      if (Number.isFinite(sample.l)) {
        minY = Math.min(minY, sample.l);
        maxY = Math.max(maxY, sample.l);
      }
      if (Number.isFinite(sample.f)) {
        minY = Math.min(minY, sample.f);
        maxY = Math.max(maxY, sample.f);
      }
    });
  });

  if (!Number.isFinite(minY) || !Number.isFinite(maxY)) {
    return null;
  }

  if (minY === maxY) {
    minY -= 1;
    maxY += 1;
  }

  return { minY, maxY };
}

function drawLine(ctx, history, minTs, maxTs, minY, maxY, width, height, key) {
  let started = false;
  ctx.beginPath();

  history.forEach((sample) => {
    const value = sample[key];
    if (!Number.isFinite(value)) {
      return;
    }

    const x = CHART_MARGIN + ((sample.t - minTs) / (maxTs - minTs)) * (width - (CHART_MARGIN * 2));
    const y = height - CHART_MARGIN - ((value - minY) / (maxY - minY)) * (height - (CHART_MARGIN * 2));

    if (!started) {
      ctx.moveTo(x, y);
      started = true;
      return;
    }
    ctx.lineTo(x, y);
  });

  if (started) {
    ctx.stroke();
  }
}

function renderLegend(servoIds) {
  const legendNode = document.getElementById("teleopChartLegend");
  if (!legendNode) {
    return;
  }

  if (servoIds.length === 0) {
    legendNode.textContent = "No data yet.";
    return;
  }

  legendNode.innerHTML = "";
  servoIds.forEach((servoId) => {
    const color = servoColor(servoId);
    const item = document.createElement("span");
    item.textContent = `ID ${servoId}`;
    item.style.color = color;
    legendNode.appendChild(item);
  });
}

function renderChart(cards) {
  const canvas = document.getElementById("teleopPositionChart");
  if (!(canvas instanceof HTMLCanvasElement)) {
    return;
  }

  const displayWidth = Math.max(320, Math.floor(canvas.clientWidth || 960));
  const displayHeight = Math.max(220, Math.floor(canvas.clientHeight || 280));
  const dpr = window.devicePixelRatio || 1;
  const bufferWidth = Math.floor(displayWidth * dpr);
  const bufferHeight = Math.floor(displayHeight * dpr);

  if (canvas.width !== bufferWidth || canvas.height !== bufferHeight) {
    canvas.width = bufferWidth;
    canvas.height = bufferHeight;
  }

  const ctx = canvas.getContext("2d");
  if (!ctx) {
    return;
  }

  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  ctx.clearRect(0, 0, displayWidth, displayHeight);

  const trackedServoIds = [];
  cards.forEach((card) => {
    const servoId = Number(card?.servo_id || 0);
    if (!servoId || trackedServoIds.includes(servoId)) {
      return;
    }
    if (!card?.mapped) {
      return;
    }
    trackedServoIds.push(servoId);
  });

  const servoIds = trackedServoIds.slice(0, 6);
  renderLegend(servoIds);
  renderLagMetrics(historyByServoId, servoIds);

  const now = Date.now();
  const minTs = now - CHART_WINDOW_MS;
  const maxTs = now;
  const domain = collectChartDomain(servoIds);

  ctx.strokeStyle = "rgba(148, 163, 184, 0.35)";
  ctx.lineWidth = 1;
  ctx.strokeRect(CHART_MARGIN, CHART_MARGIN, displayWidth - (CHART_MARGIN * 2), displayHeight - (CHART_MARGIN * 2));

  if (!domain) {
    ctx.fillStyle = "rgba(148, 163, 184, 0.8)";
    ctx.font = "12px Space Grotesk, sans-serif";
    ctx.fillText("Waiting for servo telemetry...", CHART_MARGIN + 6, displayHeight / 2);
    return;
  }

  const { minY, maxY } = domain;
  ctx.fillStyle = "rgba(148, 163, 184, 0.9)";
  ctx.font = "11px Space Grotesk, sans-serif";
  ctx.fillText(`${Math.round(maxY)}`, 4, CHART_MARGIN + 4);
  ctx.fillText(`${Math.round(minY)}`, 4, displayHeight - CHART_MARGIN);
  ctx.fillText("Leader: solid / Follower: dashed", CHART_MARGIN + 6, 16);

  servoIds.forEach((servoId) => {
    const history = historyByServoId.get(servoId) || [];
    if (history.length < 2) {
      return;
    }

    const color = servoColor(servoId);
    ctx.strokeStyle = color;
    ctx.lineWidth = 2;
    ctx.setLineDash([]);
    drawLine(ctx, history, minTs, maxTs, minY, maxY, displayWidth, displayHeight, "l");

    ctx.strokeStyle = color;
    ctx.lineWidth = 1.5;
    ctx.setLineDash([5, 4]);
    drawLine(ctx, history, minTs, maxTs, minY, maxY, displayWidth, displayHeight, "f");
    ctx.setLineDash([]);
  });
}

export function renderTeleopState(data) {
  const summary = data?.summary || {};
  const config = data?.config || {};
  const runtime = data?.runtime || {};
  const cards = Array.isArray(data?.cards) ? data.cards : [];

  appendHistory(cards);
  renderChart(cards);

  document.getElementById("teleopMatchedCount").textContent = `${summary.matched || 0}`;
  document.getElementById("teleopMirrorableCount").textContent = `${summary.mirrorable || 0}`;
  document.getElementById("teleopLeaderDetected").textContent = `${summary.leader_detected || 0}`;
  document.getElementById("teleopFollowerDetected").textContent = `${summary.follower_detected || 0}`;
  const continuousNode = document.getElementById("teleopContinuousState");
  const continuousEnabled = !!runtime.continuous_enabled;
  const continuousServoId = Number(runtime.continuous_servo_id || 0);
  const fwLastMs = Number(runtime.fw_latency_last_ms || 0);
  const fwEwmaMs = Number(runtime.fw_latency_ewma_ms || 0);
  const fwP95Ms = Number(runtime.fw_latency_p95_ms || 0);
  const fwPendingCount = Number(runtime.fw_pending_count || 0);
  const fwTimeoutCount = Number(runtime.fw_timeout_count || 0);
  const transportMode = Number(runtime.transport_mode ?? config.transport_mode ?? 0);
  continuousNode.textContent = continuousEnabled
    ? (continuousServoId === 0 ? "all matched" : `ID ${continuousServoId}`)
    : "off";
  continuousNode.classList.toggle("state-on", continuousEnabled);
  continuousNode.dataset.enabled = continuousEnabled ? "1" : "0";
  continuousNode.dataset.servoId = `${continuousServoId}`;
  const fwLatencyNode = document.getElementById("teleopFwLatency");
  if (fwLatencyNode) {
    fwLatencyNode.textContent = `last ${fwLastMs} / ewma ${fwEwmaMs} / p95 ${fwP95Ms} ms`;
  }
  const fwQueueNode = document.getElementById("teleopFwQueue");
  if (fwQueueNode) {
    fwQueueNode.textContent = `pending ${fwPendingCount} / timeout ${fwTimeoutCount}`;
  }
  const transportNode = document.getElementById("teleopTransportMode");
  if (transportNode) {
    transportNode.textContent = transportMode === 1 ? "Wi-Fi UDP" : "ESP-NOW";
  }
  syncControl("teleopEnabledInput", !!config.enabled, "checked");
  syncControl("teleopSameIdInput", !!config.same_id_mapping, "checked");
  syncControl("teleopSpeedInput", `${config.speed_pct ?? 35}`);
  syncControl("teleopTransportInput", `${transportMode}`);

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