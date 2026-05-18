function modeLabel(v) {
  switch (v) {
    case 0: return 'Idle';
    case 1: return 'CalibrationLeader';
    case 2: return 'CalibrationFollower';
    case 3: return 'Teleoperation';
    default: return 'Unknown';
  }
}

function stateLabel(v) {
  switch (v) {
    case 0: return 'PairingOrUnpaired';
    case 1: return 'Paired';
    case 2: return 'WaitingCalibration';
    case 3: return 'WaitingEspNow';
    case 4: return 'Ready';
    default: return 'Unknown';
  }
}

async function refresh() {
  try {
    const response = await fetch('/api/latest', { cache: 'no-store' });
    const data = await response.json();

    document.getElementById('connected').textContent = data.connected ? 'yes' : 'no';
    document.getElementById('leaderIp').textContent = data.leader_ip || '-';
    document.getElementById('followerIp').textContent = data.follower_ip || '-';
    document.getElementById('mode').textContent = modeLabel(data.mode);
    document.getElementById('leaderState').textContent = stateLabel(data.leader_state);
    document.getElementById('followerState').textContent = stateLabel(data.follower_state);
    document.getElementById('status').textContent = data.status || '-';
    document.getElementById('cpu0').textContent = `${data.cpu0_load_pct}%`;
    document.getElementById('cpu1').textContent = `${data.cpu1_load_pct}%`;
    document.getElementById('uptime').textContent = `${data.uptime_ms} ms`;
  } catch (err) {
  }
}

setInterval(refresh, 200);
refresh();
