#include "leader_app.h"

#include "../Config/leader_runtime_config.h"
#include "../common/controller/controller_operation_profile.h"
#include "leader_presence_service.h"

#include <WiFi.h>

namespace soarm {

void LeaderApp::refreshEspNowRadioTransport() {
  auto *presence = static_cast<LeaderPresenceService *>(presenceService_.get());
  if (presence == nullptr) {
    return;
  }
  (void)presence->ensureEspNowTransportReady(0U);
  presence->resetTurboTeleopSession();
}

namespace {

void applyEspNowRadioRefresh(LeaderPresenceService *presence) {
  if (presence == nullptr) {
    return;
  }
  (void)presence->ensureEspNowTransportReady(0U);
  presence->resetTurboTeleopSession();
}

} // namespace

bool LeaderApp::shouldKeepHomeStaConnectedForProfile(ControllerOperationProfile profile) const {
  if (oledMenuBrowseMode_.load()) {
    return true;
  }
  if (profile != ControllerOperationProfile::TeleopEspNow &&
      profile != ControllerOperationProfile::TeleopEspNowTurbo) {
    return true;
  }
  if (espNowResyncAfterWifiDirectPending_) {
    return true;
  }
  if (homeStaChannelLearned_) {
    return false;
  }
  if ((millis() - bootMs_) >= config::leader::kHomeWifiChannelPrimeTimeoutMs) {
    return false;
  }
  return true;
}

void LeaderApp::updateEspNowStaPrime(uint32_t nowMs) {
  const ControllerOperationProfile profile =
      sanitizeControllerOperationProfile(controllerOperationProfile_.load());
  const bool onEspNowProfile =
      profile == ControllerOperationProfile::TeleopEspNow ||
      profile == ControllerOperationProfile::TeleopEspNowTurbo;

  if (espNowResyncAfterWifiDirectPending_) {
    const bool connected = WiFi.status() == WL_CONNECTED;
    const bool timedOut =
        (nowMs - espNowResyncAfterWifiDirectStartedMs_) >=
        config::leader::kPostWifiDirectEspNowResyncTimeoutMs;
    if (!connected && !timedOut) {
      return;
    }
    espNowResyncAfterWifiDirectPending_ = false;
    syncWifiRadioPolicyForProfile(profile);
    refreshEspNowRadioTransport();
    return;
  }

  if (!onEspNowProfile || homeStaChannelLearned_) {
    return;
  }

  const bool timedOut = (nowMs - bootMs_) >= config::leader::kHomeWifiChannelPrimeTimeoutMs;
  const bool connected = WiFi.status() == WL_CONNECTED;
  if (!connected && !timedOut) {
    return;
  }

  homeStaChannelLearned_ = true;
  syncWifiRadioPolicyForProfile(profile);
  refreshEspNowRadioTransport();
}

void LeaderApp::syncWifiRadioPolicyForProfile(ControllerOperationProfile profile) {
  auto *presence = static_cast<LeaderPresenceService *>(presenceService_.get());
  const bool linkEngaged = wifiDirectLinkEngaged_.load();

  if (profile == ControllerOperationProfile::OtaReady) {
    if (linkEngaged || wifiDirectSession_.isActive()) {
      disengageWifiDirectLink();
    }
    teleopContinuousEnabled_.store(false);
    wifiOta_.setStaConnectDesired(true);
    if (otaEngaged_.load()) {
      wifiOta_.restoreHomeStation();
    }
    const bool pauseDashboardTcp =
        profile == ControllerOperationProfile::TeleopEspNow ||
        profile == ControllerOperationProfile::TeleopEspNowTurbo;
    telemetryStreamServer_.setListeningEnabled(!pauseDashboardTcp);
    return;
  }

  if (linkEngaged && presence != nullptr) {
    wifiDirectSession_.tick(*presence, wifiDirectRadio_, millis());
    wifiOta_.setStaConnectDesired(false);
    (void)presence->ensureEspNowTransportReady();
  } else {
    if (wifiDirectSession_.isActive()) {
      wifiDirectSession_.end(wifiDirectRadio_, true);
    }
    const bool keepHomeSta =
        espNowResyncAfterWifiDirectPending_ ||
        (!deferHomeStaReconnect_.load() && shouldKeepHomeStaConnectedForProfile(profile));
    wifiOta_.setStaConnectDesired(keepHomeSta);
    if (!keepHomeSta && presence != nullptr) {
      (void)presence->ensureEspNowTransportReady();
    }
  }

  const bool pauseDashboardTcp =
      profile == ControllerOperationProfile::TeleopEspNow ||
      profile == ControllerOperationProfile::TeleopEspNowTurbo;
  telemetryStreamServer_.setListeningEnabled(!pauseDashboardTcp);
}

void LeaderApp::engageOtaMode() {
  const ControllerOperationProfile profile =
      sanitizeControllerOperationProfile(controllerOperationProfile_.load());
  if (profile != ControllerOperationProfile::OtaReady || otaEngaged_.load()) {
    return;
  }

  disengageWifiDirectLink();
  if (auto *presence = static_cast<LeaderPresenceService *>(presenceService_.get())) {
    (void)presence->ensureEspNowTransportReady();
  }
  otaEngaged_.store(true);
  teleopContinuousEnabled_.store(false);
  wifiDirectTeleopActive_.store(false);
  xboxControllerService_.discardPendingButtonPress(XboxLogicalButton::A);
  wifiOta_.restoreHomeStation();
  syncWifiRadioPolicyForProfile(profile);
  lastOledRefreshMs_ = 0U;
  setTransientStatus("OTA active: flash now", config::leader::kMoveStatusHoldMs);
}

void LeaderApp::engageWifiDirectLink() {
  const ControllerOperationProfile profile =
      sanitizeControllerOperationProfile(controllerOperationProfile_.load());
  if (profile != ControllerOperationProfile::TeleopWifi || wifiDirectLinkEngaged_.load()) {
    return;
  }

  auto *presence = static_cast<LeaderPresenceService *>(presenceService_.get());
  if (presence == nullptr) {
    return;
  }

  wifiDirectTeleopActive_.store(false);
  teleopContinuousEnabled_.store(false);
  wifiDirectLinkEngaged_.store(true);
  xboxControllerService_.discardPendingButtonPress(XboxLogicalButton::A);
  lastOledRefreshMs_ = 0U;

  wifiDirectSession_.begin(*presence, wifiDirectRadio_);
  syncWifiRadioPolicyForProfile(profile);
  setTransientStatus("wifi direct: AP up", config::leader::kMoveStatusHoldMs);
}

void LeaderApp::disengageWifiDirectLink() {
  if (!wifiDirectLinkEngaged_.load() && !wifiDirectSession_.isActive()) {
    return;
  }

  auto *presence = static_cast<LeaderPresenceService *>(presenceService_.get());
  if (presence != nullptr) {
    (void)presence->sendWifiDirectSessionEnd();
  }

  wifiDirectLinkEngaged_.store(false);
  wifiDirectTeleopActive_.store(false);
  teleopContinuousEnabled_.store(false);
  wifiDirectSession_.end(wifiDirectRadio_, true);

  espNowResyncAfterWifiDirectPending_ = true;
  espNowResyncAfterWifiDirectStartedMs_ = millis();
  deferHomeStaReconnect_.store(false);
  wifiOta_.restoreHomeStation();

  const ControllerOperationProfile profile =
      sanitizeControllerOperationProfile(controllerOperationProfile_.load());
  syncWifiRadioPolicyForProfile(profile);
  lastOledRefreshMs_ = 0U;
  setTransientStatus("wifi direct: router STA", config::leader::kMoveStatusHoldMs);
}

void LeaderApp::handleOtaButtons(bool confirmPressed, bool validatePressed) {
  if (!otaEngaged_.load()) {
    if (confirmPressed) {
      engageOtaMode();
    }
    if (validatePressed) {
      applyControllerOperationProfile(toProfileRaw(ControllerOperationProfile::TeleopEspNow));
      setTransientStatus("OTA skipped", config::leader::kMoveStatusHoldMs);
    }
    return;
  }

  if (validatePressed) {
    otaEngaged_.store(false);
    applyControllerOperationProfile(toProfileRaw(ControllerOperationProfile::TeleopEspNow));
    setTransientStatus("OTA done", config::leader::kMoveStatusHoldMs);
  }
}

void LeaderApp::handleTeleopWifiButtons(bool confirmPressed, bool validatePressed) {
  if (!wifiDirectLinkEngaged_.load()) {
    if (confirmPressed) {
      engageWifiDirectLink();
    }
    if (validatePressed) {
      applyControllerOperationProfile(toProfileRaw(ControllerOperationProfile::TeleopEspNow));
      restoreOledMenuBrowseMode();
      setTransientStatus("wifi teleop skipped", config::leader::kMoveStatusHoldMs);
    }
    return;
  }

  if (!wifiDirectSession_.isFollowerReady()) {
    if (validatePressed) {
      disengageWifiDirectLink();
      restoreOledMenuBrowseMode();
      setTransientStatus("wifi direct canceled", config::leader::kMoveStatusHoldMs);
    }
    return;
  }

  if (!wifiDirectTeleopActive_.load()) {
    if (confirmPressed) {
      wifiDirectTeleopActive_.store(true);
      teleopContinuousEnabled_.store(true);
      setTransientStatus("teleop wifi start", config::leader::kMoveStatusHoldMs);
      lastOledRefreshMs_ = 0U;
    }
    if (validatePressed) {
      disengageWifiDirectLink();
      restoreOledMenuBrowseMode();
      setTransientStatus("wifi direct canceled", config::leader::kMoveStatusHoldMs);
    }
    return;
  }

  if (validatePressed) {
    wifiDirectTeleopActive_.store(false);
    releaseFollowerTeleopHold();
    disengageWifiDirectLink();
    applyControllerOperationProfile(toProfileRaw(ControllerOperationProfile::TeleopEspNow));
    restoreOledMenuBrowseMode();
    setTransientStatus("teleop wifi stop", config::leader::kMoveStatusHoldMs);
  }
}

} // namespace soarm
