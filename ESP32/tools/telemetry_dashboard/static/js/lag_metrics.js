const LAG_MAX_SAMPLES = 24;

function estimateSamplePeriodMs(history) {
  const dts = [];
  for (let i = 1; i < history.length; i += 1) {
    const dt = history[i].t - history[i - 1].t;
    if (Number.isFinite(dt) && dt > 0) {
      dts.push(dt);
    }
  }
  if (dts.length === 0) {
    return null;
  }
  dts.sort((a, b) => a - b);
  return dts[Math.floor(dts.length / 2)];
}

function collectAlignedDeltas(history, lagSamples) {
  const deltas = [];
  for (let i = 0; i < history.length; i += 1) {
    const follower = history[i]?.f;
    const leaderIndex = i - lagSamples;
    if (leaderIndex < 0 || leaderIndex >= history.length) {
      continue;
    }

    const leader = history[leaderIndex]?.l;
    if (!Number.isFinite(leader) || !Number.isFinite(follower)) {
      continue;
    }

    deltas.push(follower - leader);
  }
  return deltas;
}

function summarizeDeltaAbs(deltas) {
  if (deltas.length === 0) {
    return null;
  }

  const absValues = deltas.map((v) => Math.abs(v)).sort((a, b) => a - b);
  const sum = absValues.reduce((acc, v) => acc + v, 0);
  const mean = sum / absValues.length;
  const p95Index = Math.min(absValues.length - 1, Math.floor(absValues.length * 0.95));
  const p95 = absValues[p95Index];
  return { mean, p95 };
}

function estimateLagAndError(history) {
  if (!Array.isArray(history) || history.length < 12) {
    return null;
  }

  let bestLag = 0;
  let bestScore = Number.POSITIVE_INFINITY;
  let bestCount = 0;

  for (let lag = -LAG_MAX_SAMPLES; lag <= LAG_MAX_SAMPLES; lag += 1) {
    const deltas = collectAlignedDeltas(history, lag);
    if (deltas.length < 8) {
      continue;
    }

    const absSummary = summarizeDeltaAbs(deltas);
    if (!absSummary) {
      continue;
    }

    if (absSummary.mean < bestScore) {
      bestScore = absSummary.mean;
      bestLag = lag;
      bestCount = deltas.length;
    }
  }

  if (!Number.isFinite(bestScore) || bestCount < 8) {
    return null;
  }

  const alignedDeltas = collectAlignedDeltas(history, bestLag);
  const absSummary = summarizeDeltaAbs(alignedDeltas);
  if (!absSummary) {
    return null;
  }

  const samplePeriodMs = estimateSamplePeriodMs(history);
  const lagMs = Number.isFinite(samplePeriodMs) ? (bestLag * samplePeriodMs) : null;
  return {
    lagSamples: bestLag,
    lagMs,
    meanAbsDelta: absSummary.mean,
    p95AbsDelta: absSummary.p95,
  };
}

function pickBestHistory(historyByServoId, servoIds) {
  let best = null;
  servoIds.forEach((servoId) => {
    const history = historyByServoId.get(servoId) || [];
    if (!best || history.length > best.history.length) {
      best = { servoId, history };
    }
  });
  return best;
}

export function renderLagMetrics(historyByServoId, servoIds) {
  const lagNode = document.getElementById("teleopLagEstimate");
  const errNode = document.getElementById("teleopErrorEstimate");
  if (!lagNode || !errNode) {
    return;
  }

  const best = pickBestHistory(historyByServoId, servoIds);
  if (!best || best.history.length < 12) {
    lagNode.textContent = "-";
    errNode.textContent = "-";
    return;
  }

  const estimate = estimateLagAndError(best.history);
  if (!estimate) {
    lagNode.textContent = "-";
    errNode.textContent = "-";
    return;
  }

  if (Number.isFinite(estimate.lagMs)) {
    const lagSamplesLabel = `${estimate.lagSamples >= 0 ? "+" : ""}${estimate.lagSamples}`;
    lagNode.textContent = `${Math.round(estimate.lagMs)} ms (${lagSamplesLabel} smp)`;
  } else {
    lagNode.textContent = `${estimate.lagSamples >= 0 ? "+" : ""}${estimate.lagSamples} smp`;
  }

  const p95Pct = (estimate.p95AbsDelta / 4095) * 100;
  errNode.textContent = `mean ${estimate.meanAbsDelta.toFixed(1)} / p95 ${estimate.p95AbsDelta.toFixed(1)} (${p95Pct.toFixed(2)}%)`;
}
