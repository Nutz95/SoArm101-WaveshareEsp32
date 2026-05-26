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

const REFRESH_INTERVAL_MS = 100;
const VIEW_PARTIALS = ["views/pairing.html", "views/overview.html", "views/teleop.html", "views/manual.html"];

async function loadViewPartials() {
  const root = document.getElementById("dashboardViewsRoot");
  if (!root) {
    throw new Error("dashboardViewsRoot not found");
  }

  const partialMarkup = await Promise.all(
    VIEW_PARTIALS.map(async (path) => {
      const response = await fetch(path, { cache: "no-cache" });
      if (!response.ok) {
        throw new Error(`Failed to load ${path}`);
      }
      return response.text();
    })
  );

  root.innerHTML = partialMarkup.join("\n");
}

async function refresh() {
  try {
    const [data, teleopState] = await Promise.all([fetchLatest(), fetchTeleopState()]);
    renderSnapshot(data);
    renderTeleopState(teleopState);
    syncPendingCommandStatus(data);
  } catch (_err) {
  }
}

async function bootstrap() {
  await loadViewPartials();
  initNavigationView();
  initPairingView(commandWithStatus, saveTeleopConfig, refresh);
  initTeleopView(commandWithStatus, saveTeleopConfig, triggerTeleopMirror, refresh);
  initManualView(commandWithStatus, hasPendingFollowerCommand, registerPendingCommand);
  setInterval(refresh, REFRESH_INTERVAL_MS);
  refresh();
}

bootstrap().catch(() => {
});
