import {
  fetchLatest,
  fetchTeleopState,
  fetchControllerConfig,
  commandWithStatus,
  saveTeleopConfig,
  saveControllerConfig,
  triggerTeleopMirror,
} from "./js/api.js";
import { renderSnapshot } from "./js/dashboard_render.js";
import { renderTeleopState } from "./js/teleop_ui.js";
import { initCalibrationView } from "./js/views/calibration_view.js";
import { initManualView } from "./js/views/manual_view.js";
import { initNavigationView } from "./js/views/navigation_view.js";
import { initPairingView } from "./js/views/pairing_view.js";
import { initTeleopView } from "./js/views/teleop_view.js";
import { initModeView, renderOperationModePanel } from "./js/views/mode_view.js";
import {
  hasPendingFollowerCommand,
  registerPendingCommand,
  syncPendingCommandStatus,
} from "./js/pending_command.js";

const TELEOP_REFRESH_INTERVAL_MS = 100;
const DEFAULT_REFRESH_INTERVAL_MS = 250;
const VIEW_PARTIALS = [
  "views/mode/mode_selection.html",
  "views/pairing/pairing_control.html",
  "views/pairing/xbox_pairing.html",
  "views/calibration/workflow.html",
  "views/overview/command_ack.html",
  "views/overview/transport_diagnostics.html",
  "views/overview/runtime.html",
  "views/overview/servo_buses.html",
  "views/teleop/summary.html",
  "views/teleop/controls.html",
  "views/teleop/chart.html",
  "views/teleop/cards.html",
  "views/manual/commands.html",
];

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
    renderOperationModePanel(data);
    renderTeleopState(teleopState);
    syncPendingCommandStatus(data);
  } catch (_err) {
  }
}

function refreshIntervalMs() {
  const activeTab = document.querySelector("[data-view-tab].is-active")?.dataset?.viewTab;
  return activeTab === "teleop" ? TELEOP_REFRESH_INTERVAL_MS : DEFAULT_REFRESH_INTERVAL_MS;
}

function scheduleRefresh() {
  window.setTimeout(async () => {
    await refresh();
    scheduleRefresh();
  }, refreshIntervalMs());
}

async function bootstrap() {
  await loadViewPartials();
  initNavigationView();
  initModeView(refresh);
  initPairingView(commandWithStatus, saveTeleopConfig, fetchControllerConfig, saveControllerConfig, refresh);
  initCalibrationView(commandWithStatus, saveTeleopConfig, refresh);
  initTeleopView(commandWithStatus, saveTeleopConfig, triggerTeleopMirror, refresh);
  initManualView(commandWithStatus, hasPendingFollowerCommand, registerPendingCommand);
  refresh();
  scheduleRefresh();
}

bootstrap().catch(() => {
});
