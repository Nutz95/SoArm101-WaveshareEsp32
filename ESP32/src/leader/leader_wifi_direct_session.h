#pragma once

#include "../common/wifi/wifi_direct_radio_service.h"
#include "../common/wifi/wifi_direct_session.h"

#include <cstdint>

namespace soarm {

class LeaderPresenceService;

class LeaderWifiDirectSession {
public:
  void begin(LeaderPresenceService &presence, WifiDirectRadioService &radio);
  void end(WifiDirectRadioService &radio, bool restoreHomeSta);
  void tick(LeaderPresenceService &presence, WifiDirectRadioService &radio, uint32_t nowMs);
  bool isActive() const;
  bool isFollowerReady() const;
  const char *followerTeleopIp() const;
  const char *leaderApIp() const;

private:
  bool sendOffer(LeaderPresenceService &presence);
  void handleAckTimeout(LeaderPresenceService &presence, uint32_t nowMs);

  LeaderPresenceService *presence_{nullptr};
  WifiDirectRadioService *radio_{nullptr};
  WifiDirectCredentials credentials_{};
  uint32_t activeSessionId_{0U};
  uint32_t offerSentMs_{0U};
  uint32_t lastOfferResendMs_{0U};
  char followerTeleopIp_[16]{};
  bool offerSent_{false};
  bool followerReady_{false};
};

} // namespace soarm
