import { fetchLatest, fetchTeleopState, commandWithStatus, saveTeleopConfig, triggerTeleopMirror } from "./js/api.js";
import { renderSnapshot } from "./js/dashboard_render.js";
import { renderTeleopState } from "./js/teleop_ui.js";
import { initManualView } from "./js/views/manual_view.js";
import { initNavigationView } from "./js/views/navigation_view.js";
import { initPairingView } from "./js/views/pairing_view.js";
import { initTeleopView } from "./js/views/teleop_view.js";
import {
  hasPendingFollowerCommand,
  registerPendingCommand,
  syncPendingCommandStatus,
} from "./js/pending_command.js";

const REFRESH_INTERVAL_MS = 200;

async function refresh() {
  try {
    const [data, teleopState] = await Promise.all([fetchLatest(), fetchTeleopState()]);
    renderSnapshot(data);
    renderTeleopState(teleopState);
    syncPendingCommandStatus(data);
  } catch (_err) {
  }
}

initNavigationView();
initPairingView(commandWithStatus);
initTeleopView(commandWithStatus, saveTeleopConfig, triggerTeleopMirror, refresh);
initManualView(commandWithStatus, hasPendingFollowerCommand, registerPendingCommand);
setInterval(refresh, REFRESH_INTERVAL_MS);
refresh();
