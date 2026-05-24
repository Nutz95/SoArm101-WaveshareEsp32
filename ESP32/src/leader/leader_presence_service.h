#pragma once

#include "../common/interfaces/i_leader_presence_service.h"
#include "../common/mac_address_utils.h"
#include "../common/presence/espnow_presence_base.h"
#include "../common/presence/presence_packet.h"
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
  bool requestServoScan(uint16_t requestId) override;
  bool requestServoControl(uint8_t op, uint32_t value, uint16_t requestId) override;
  const char *followerServoIds() const override;
  const char *followerServoTelemetry() const override;
  uint8_t followerServoCount() const override;
  bool followerServoDebugManual() const override;
  uint16_t followerLastAckRequestId() const override;
  uint8_t followerLastAckCommandOp() const override;
  uint8_t followerLastAckStatus() const override;
  uint32_t followerLastAckMs() const override;

private:
  void onPresenceFrame(const uint8_t *mac, const uint8_t *data, int len) override;

  // Per-message-type handlers dispatched from onPresenceFrame.
  void handlePairRequest(const uint8_t *mac, const PresencePacket &packet);
  void handlePresenceData(const uint8_t *mac, const PresencePacket &packet);
  void handleServoCommandAck(const uint8_t *mac, const PresencePacket &packet);

  void sendPairAck(const uint8_t mac[6]);
  void sendPairResetTo(const uint8_t mac[6]);
  void sendPairResetBroadcast();
  void sendServoScanBroadcast(uint16_t requestId);
  bool sendServoControl(const uint8_t mac[6], uint8_t op, uint32_t value, uint16_t requestId);
  bool sendServoControlBroadcast(uint8_t op, uint32_t value, uint16_t requestId);
  bool addBroadcastPeer();
  bool isPairedMac(const uint8_t mac[6]) const;

  PeerPairingStore pairingStore_;
  bool started_{false};
  bool hasPairedMac_{false};
  uint8_t pairedFollowerMac_[6]{};
  uint32_t lastFollowerSeenMs_{0};
  uint8_t followerServoCount_{0};
  bool followerServoDebugManual_{false};
  uint16_t followerLastAckRequestId_{0U};
  uint8_t followerLastAckCommandOp_{0U};
  uint8_t followerLastAckStatus_{0U};
  uint32_t followerLastAckMs_{0U};
  // Scheduled PairReset broadcasts: sends remaining count one-per-tick at 100 ms intervals.
  uint8_t pendingResetBroadcastCount_{0U};
  uint32_t nextResetBroadcastMs_{0U};
  char followerIp_[16]{};
  char followerServoIdsText_[48]{};
  char followerServoTelemetryText_[96]{};
  char pairedFollowerMacText_[18]{};
  char localMacText_[18]{};
};

} // namespace soarm
