#include "follower_app.h"

#include "follower_presence_service.h"

namespace soarm {

void FollowerApp::syncWifiRadioPolicy(uint32_t nowMs) {
  auto *presence = static_cast<FollowerPresenceService *>(presenceService_.get());
  bool restoredHomeStaThisTick = false;
  if (presence != nullptr) {
    if (presence->consumeWifiDirectSessionEnd()) {
      followerWifiDirectLink_.reset(*presence, wifiDirectRadio_, wifiOta_, true);
      wifiOta_.restoreHomeStation();
      restoredHomeStaThisTick = true;
      (void)presence->ensureEspNowTransportReady();
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

  if (followerWifiDirectLink_.isActive()) {
    wifiOta_.setStaConnectDesired(false);
    if (presence != nullptr) {
      (void)presence->ensureEspNowTransportReady();
    }
    return;
  }

  const bool preferHomeSta = presence == nullptr || presence->preferWifiStaConnected(nowMs);
  const bool keepHomeSta = restoredHomeStaThisTick || preferHomeSta;
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
