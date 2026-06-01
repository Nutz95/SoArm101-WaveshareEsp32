#include "follower_app.h"

#include "follower_presence_service.h"

namespace soarm {

void FollowerApp::syncWifiRadioPolicy(uint32_t nowMs) {
  auto *presence = static_cast<FollowerPresenceService *>(presenceService_.get());
  if (presence != nullptr) {
    if (presence->consumeWifiDirectSessionEnd()) {
      followerWifiDirectLink_.reset(*presence, wifiDirectRadio_, wifiOta_, true);
    }

    WifiDirectCredentials offer{};
    if (presence->consumeWifiDirectOffer(offer)) {
      followerWifiDirectLink_.acceptOffer(offer);
    }
    followerWifiDirectLink_.tick(*presence, wifiDirectRadio_, wifiOta_, nowMs);
  }

  if (followerWifiDirectLink_.isActive()) {
    wifiOta_.setStaConnectDesired(false);
    return;
  }

  wifiOta_.setStaConnectDesired(true);
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
