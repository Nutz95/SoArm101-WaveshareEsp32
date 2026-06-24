#include "follower_app.h"

#include "../Config/follower_runtime_config.h"
#include "follower_presence_service.h"

#include <WiFi.h>

namespace soarm {

void FollowerApp::updateEspNowResyncAfterWifiDirect(uint32_t nowMs) {
  if (!espNowResyncAfterWifiDirectPending_) {
    return;
  }

  const bool connected = WiFi.status() == WL_CONNECTED;
  const bool timedOut =
      (nowMs - espNowResyncAfterWifiDirectStartedMs_) >=
      config::follower::kPostWifiDirectEspNowResyncTimeoutMs;
  if (!connected && !timedOut) {
    return;
  }

  espNowResyncAfterWifiDirectPending_ = false;
  if (auto *presence = static_cast<FollowerPresenceService *>(presenceService_.get())) {
    (void)presence->ensureEspNowTransportReady(0U);
  }
}

void FollowerApp::syncWifiRadioPolicy(uint32_t nowMs) {
  auto *presence = static_cast<FollowerPresenceService *>(presenceService_.get());
  bool restoredHomeStaThisTick = false;
  if (presence != nullptr) {
    if (presence->consumeWifiDirectSessionEnd()) {
      followerWifiDirectLink_.reset(*presence, wifiDirectRadio_, wifiOta_, true);
      wifiOta_.restoreHomeStation();
      presence->resetTeleopTransportState();
      espNowResyncAfterWifiDirectPending_ = true;
      espNowResyncAfterWifiDirectStartedMs_ = nowMs;
      restoredHomeStaThisTick = true;
    }

    WifiDirectCredentials offer{};
    if (presence->consumeWifiDirectOffer(offer)) {
      if (followerWifiDirectLink_.shouldAcceptOffer(offer)) {
        wifiOta_.setStaConnectDesired(false);
        (void)presence->ensureEspNowTransportReady();
        followerWifiDirectLink_.acceptOffer(offer);
        presence->setWifiDirectJoinSession(offer.sessionId);
      }
    }
    followerWifiDirectLink_.tick(*presence, wifiDirectRadio_, wifiOta_, nowMs);
    if (!followerWifiDirectLink_.isActive()) {
      presence->setDirectWifiSessionActive(false);
    }
  }

  updateEspNowResyncAfterWifiDirect(nowMs);

  if (followerWifiDirectLink_.isActive()) {
    wifiOta_.setStaConnectDesired(false);
    if (presence != nullptr) {
      (void)presence->ensureEspNowTransportReady();
    }
    return;
  }

  const bool preferHomeSta = presence == nullptr || presence->preferWifiStaConnected(nowMs);
  const bool keepHomeSta =
      restoredHomeStaThisTick || espNowResyncAfterWifiDirectPending_ || preferHomeSta;
  wifiOta_.setStaConnectDesired(keepHomeSta);
  if (!keepHomeSta && presence != nullptr) {
    (void)presence->ensureEspNowTransportReady();
  }
}

const char *FollowerApp::activeWifiIpForPresence() const {
  if (followerWifiDirectLink_.isActive()) {
    const char *directIp = wifiDirectRadio_.stationIp();
    if (directIp != nullptr && directIp[0] != '\0') {
      return directIp;
    }
  }
  return wifiOta_.ipAddress();
}

} // namespace soarm
