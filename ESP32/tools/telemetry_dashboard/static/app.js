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

async function sendCommand(command, value = 0) {
  const response = await fetch('/api/command', {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
    },
    body: JSON.stringify({ command, value }),
  });
  return response.json();
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
    document.getElementById('pairingLocked').textContent = data.pairing_locked ? 'locked' : 'open';
    document.getElementById('pairingLocked').className = data.pairing_locked ? 'locked' : '';
    document.getElementById('leaderMac').textContent = data.leader_mac || '-';
    document.getElementById('followerMac').textContent = data.follower_mac || '-';
  } catch (err) {
  }
}

document.getElementById('resetPairingBtn').addEventListener('click', async () => {
  const statusNode = document.getElementById('commandStatus');
  try {
    const result = await sendCommand('reset_pairing');
    statusNode.textContent = result.ok ? 'Pairing reset command sent.' : 'Pairing reset failed (not connected).';
  } catch (err) {
    statusNode.textContent = 'Pairing reset failed.';
  }
});

document.getElementById('servoScanBtn').addEventListener('click', async () => {
  const statusNode = document.getElementById('commandStatus');
  try {
    const result = await sendCommand('servo_scan');
    statusNode.textContent = result.ok ? 'Servo scan command sent.' : 'Servo scan failed (not connected).';
  } catch (err) {
    statusNode.textContent = 'Servo scan failed.';
  }
});

setInterval(refresh, 200);
refresh();
