#pragma once

#include "../common/interfaces/i_follower_presence_service.h"
#include "../common/peer_pairing_store.h"

#include <cstdint>

namespace soarm {

class FollowerPresenceService : public IFollowerPresenceService {
public:
  FollowerPresenceService();

  bool begin() override;
  void tick(const char *localIp) override;

private:
  static void onDataRecvStatic(const uint8_t *mac, const uint8_t *data, int len);
  void onDataRecv(const uint8_t *mac, const uint8_t *data, int len);

  bool addBroadcastPeer();
  bool addPeer(const uint8_t mac[6]);
  bool hasPairedLeader() const;
  void sendPairRequest(const char *localIp);
  void sendPresence(const char *localIp);

  PeerPairingStore pairingStore_;
  bool started_{false};
  uint32_t lastTxMs_{0};
  uint8_t pairedLeaderMac_[6]{};
  bool hasPairedLeaderMac_{false};
};

} // namespace soarm
