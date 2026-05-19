#pragma once

#include "../common/interfaces/i_leader_presence_service.h"
#include "../common/mac_address_utils.h"
#include "../common/presence/espnow_presence_base.h"
#include "../common/peer_pairing_store.h"
#include "../common/servo/servo_control_opcode.h"

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
  bool requestServoScan() override;
  bool requestServoControl(uint8_t op, uint32_t value) override;
  const char *followerServoIds() const override;
  const char *followerServoTelemetry() const override;
  uint8_t followerServoCount() const override;
  bool followerServoDebugManual() const override;

private:
  void onPresenceFrame(const uint8_t *mac, const uint8_t *data, int len) override;

  void sendPairAck(const uint8_t mac[6]);
  void sendPairResetBroadcast();
  void sendServoScanBroadcast();
  void sendServoControl(const uint8_t mac[6], uint8_t op, uint32_t value);
  void sendServoControlBroadcast(uint8_t op, uint32_t value);
  bool addBroadcastPeer();
  bool isPairedMac(const uint8_t mac[6]) const;

  PeerPairingStore pairingStore_;
  bool started_{false};
  bool hasPairedMac_{false};
  uint8_t pairedFollowerMac_[6]{};
  uint32_t lastFollowerSeenMs_{0};
  uint8_t followerServoCount_{0};
  bool followerServoDebugManual_{false};
  char followerIp_[16]{};
  char followerServoIdsText_[48]{};
  char followerServoTelemetryText_[96]{};
  char pairedFollowerMacText_[18]{};
  char localMacText_[18]{};
};

} // namespace soarm
