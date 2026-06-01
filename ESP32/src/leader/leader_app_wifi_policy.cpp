#include "leader_app.h"

#include "../Config/leader_runtime_config.h"
#include "../common/controller/controller_operation_profile.h"
#include "leader_presence_service.h"

namespace soarm {

void LeaderApp::syncWifiRadioPolicyForProfile(ControllerOperationProfile profile) {
  auto *presence = static_cast<LeaderPresenceService *>(presenceService_.get());
  const bool linkEngaged = wifiDirectLinkEngaged_.load();

  if (linkEngaged && presence != nullptr) {
    wifiDirectSession_.tick(*presence, wifiDirectRadio_, millis());
    wifiOta_.setStaConnectDesired(false);
  } else {
    if (wifiDirectSession_.isActive()) {
      wifiDirectSession_.end(wifiDirectRadio_, true);
    }
    wifiOta_.setStaConnectDesired(true);
  }

  if (profile == ControllerOperationProfile::OtaReady) {
    wifiOta_.setStaConnectDesired(true);
  }

  // Phase 3: pause :9090 TCP during ESP-NOW teleop; USB CDC debug stays active.
  const bool pauseDashboardTcp = (profile == ControllerOperationProfile::TeleopEspNow);
  telemetryStreamServer_.setListeningEnabled(!pauseDashboardTcp);
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

  wifiDirectLinkEngaged_.store(false);
  wifiDirectTeleopActive_.store(false);
  teleopContinuousEnabled_.store(false);
  wifiDirectSession_.end(wifiDirectRadio_, true);

  const ControllerOperationProfile profile =
      sanitizeControllerOperationProfile(controllerOperationProfile_.load());
  syncWifiRadioPolicyForProfile(profile);
  lastOledRefreshMs_ = 0U;
  setTransientStatus("wifi direct: router STA", config::leader::kMoveStatusHoldMs);
}

void LeaderApp::handleTeleopWifiButtons(bool confirmPressed, bool validatePressed) {
  if (!wifiDirectLinkEngaged_.load()) {
    if (confirmPressed) {
      engageWifiDirectLink();
    }
    if (validatePressed) {
      applyControllerOperationProfile(toProfileRaw(ControllerOperationProfile::TeleopEspNow));
      setTransientStatus("wifi teleop skipped", config::leader::kMoveStatusHoldMs);
    }
    return;
  }

  if (!wifiDirectSession_.isFollowerReady()) {
    if (validatePressed) {
      disengageWifiDirectLink();
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
      setTransientStatus("wifi direct canceled", config::leader::kMoveStatusHoldMs);
    }
    return;
  }

  if (validatePressed) {
    wifiDirectTeleopActive_.store(false);
    teleopContinuousEnabled_.store(false);
    disengageWifiDirectLink();
    applyControllerOperationProfile(toProfileRaw(ControllerOperationProfile::TeleopEspNow));
    setTransientStatus("teleop wifi stop", config::leader::kMoveStatusHoldMs);
  }
}

} // namespace soarm
