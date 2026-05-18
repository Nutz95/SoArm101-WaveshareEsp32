#pragma once

#include "../common/interfaces/i_leader_presence_service.h"
#include "../common/peer_pairing_store.h"

#include <cstdint>

namespace soarm {

class LeaderPresenceService : public ILeaderPresenceService {
public:
  LeaderPresenceService();

  bool begin() override;
  void tick() override;
  bool isFollowerLinked() const override;
  bool hasValidFollowerIp() const override;
  const char *followerIp() const override;

private:
  static void onDataRecvStatic(const uint8_t *mac, const uint8_t *data, int len);
  void onDataRecv(const uint8_t *mac, const uint8_t *data, int len);

  bool addPeer(const uint8_t mac[6]);
  void sendPairAck(const uint8_t mac[6]);
  bool isPairedMac(const uint8_t mac[6]) const;

  PeerPairingStore pairingStore_;
  bool started_{false};
  bool hasPairedMac_{false};
  uint8_t pairedFollowerMac_[6]{};
  uint32_t lastFollowerSeenMs_{0};
  char followerIp_[16]{};
};

} // namespace soarm
