#pragma once

#include "../common/interfaces/i_follower_presence_service.h"
#include "../common/mac_address_utils.h"
#include "../common/presence/espnow_presence_base.h"
#include "../common/peer_pairing_store.h"
#include "../common/servo/servo_control_opcode.h"

#include <cstdint>

namespace soarm {

class FollowerPresenceService : public IFollowerPresenceService, protected EspNowPresenceBase {
public:
  FollowerPresenceService();

  bool begin() override;
  void tick(const char *localIp) override;
  bool isPaired() const override;
  const char *pairedPeerMac() const override;
  const char *localMac() const override;
  bool resetPairing() override;
  bool consumeServoScanRequested(uint16_t &requestId) override;
  bool consumeServoControl(uint8_t &op, uint32_t &value, uint16_t &requestId) override;
  void updateLastCommandAck(uint16_t requestId, uint8_t op, uint8_t status) override;
  void requestImmediatePresenceTx() override;
  void updateServoTelemetry(
      const char *servoIds,
      const char *servoTelemetry,
      uint8_t servoCount,
      bool debugManual) override;

private:
  void onPresenceFrame(const uint8_t *mac, const uint8_t *data, int len) override;

  bool addBroadcastPeer();
  bool addPeer(const uint8_t mac[6]);
  bool hasPairedLeader() const;
  void sendPairRequest(const char *localIp);
  void sendPresence(const char *localIp);

  bool servoScanRequested_{false};
  uint16_t pendingServoScanRequestId_{0U};
  bool servoControlRequested_{false};
  uint8_t pendingServoControlOp_{0U};
  uint32_t pendingServoControlValue_{0U};
  uint16_t pendingServoControlRequestId_{0U};
  uint16_t lastAckRequestId_{0U};
  uint8_t lastAckCommandOp_{0U};
  uint8_t lastAckStatus_{0U};
  uint8_t servoCount_{0U};
  bool servoDebugManual_{false};
  char servoIdsText_[48]{};
  char servoTelemetryText_[96]{};

  PeerPairingStore pairingStore_;
  bool started_{false};
  uint32_t lastTxMs_{0};
  uint32_t lastPairRequestMs_{0};
  uint8_t pairedLeaderMac_[6]{};
  bool hasPairedLeaderMac_{false};
  char pairedLeaderMacText_[18]{};
  char localMacText_[18]{};
  char lastLocalIp_[16]{};
  bool forcePresenceTx_{false};
};

} // namespace soarm
