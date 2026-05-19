#pragma once

#include "../common/interfaces/i_leader_presence_service.h"
#include "../common/presence/espnow_presence_base.h"
#include "../common/peer_pairing_store.h"

#include <cstdint>

namespace soarm {

class LeaderPresenceService : public ILeaderPresenceService, protected EspNowPresenceBase {
public:
  LeaderPresenceService();

  bool begin() override;
  void tick() override;
  bool isFollowerLinked() const override;
  bool hasValidFollowerIp() const override;
  const char *followerIp() const override;
  bool isPaired() const override;
  const char *pairedPeerMac() const override;
  const char *localMac() const override;
  bool resetPairing() override;

private:
  void onPresenceFrame(const uint8_t *mac, const uint8_t *data, int len) override;

  void formatMac(const uint8_t mac[6], char out[18]) const;
  void sendPairAck(const uint8_t mac[6]);
  bool isPairedMac(const uint8_t mac[6]) const;

  PeerPairingStore pairingStore_;
  bool started_{false};
  bool hasPairedMac_{false};
  uint8_t pairedFollowerMac_[6]{};
  uint32_t lastFollowerSeenMs_{0};
  char followerIp_[16]{};
  char pairedFollowerMacText_[18]{};
  char localMacText_[18]{};
};

} // namespace soarm
