#pragma once

#include "../common/wifi/wifi_direct_radio_service.h"
#include "../common/wifi/wifi_direct_session.h"

#include <cstdint>

namespace soarm {

class FollowerPresenceService;
class WifiOtaService;

class FollowerWifiDirectLink {
public:
  bool shouldAcceptOffer(const WifiDirectCredentials &credentials) const;
  void acceptOffer(const WifiDirectCredentials &credentials);
  void tick(FollowerPresenceService &presence, WifiDirectRadioService &radio, WifiOtaService &wifiOta, uint32_t nowMs);
  bool isActive() const;
  void reset(
      FollowerPresenceService &presence,
      WifiDirectRadioService &radio,
      WifiOtaService &wifiOta,
      bool restoreHomeSta);

private:
  void trySendAck(FollowerPresenceService &presence, WifiDirectRadioService &radio, const char *stationIp);

  WifiDirectCredentials credentials_{};
  uint32_t connectStartedMs_{0U};
  bool offerPending_{false};
  bool staReady_{false};
  bool ackSent_{false};
  uint8_t joinAttemptCount_{0U};
};

} // namespace soarm
