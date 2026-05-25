#pragma once

#include "../common/interfaces/i_follower_presence_service.h"
#include "../Config/follower_runtime_config.h"
#include "../common/mac_address_utils.h"
#include "../common/presence/espnow_presence_base.h"
#include "../common/presence/presence_packet.h"
#include "../common/peer_pairing_store.h"
#include "../common/servo/servo_control_opcode.h"
#include "../Config/common_runtime_config.h"

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
  bool consumeTeleopMirrorBatch(
      uint8_t *ids,
      int16_t *positions,
      uint8_t capacity,
      uint8_t &count,
      uint8_t &speedPct,
      uint16_t &requestId) override;
  void updateLastCommandAck(uint16_t requestId, uint8_t op, uint8_t status) override;
  void requestImmediatePresenceTx() override;
  void updateServoTelemetry(
      const char *servoIds,
      const char *servoTelemetry,
      uint8_t servoCount,
      bool debugManual) override;

private:
  struct PendingServoControl {
    uint8_t op;
    uint32_t value;
    uint16_t requestId;
    uint8_t sequence;
  };

  void onPresenceFrame(const uint8_t *mac, const uint8_t *data, int len) override;
  void handlePairResetMessage(const uint8_t *mac, const PresencePacket &packet);
  void handleServoScanMessage(const uint8_t *mac, const PresencePacket &packet);
  void handleServoControlMessage(const uint8_t *mac, const PresencePacket &packet);
  void handleServoControlBatchMessage(const uint8_t *mac, const PresencePacket &packet);
  void handlePairAckMessage(const uint8_t *mac, const PresencePacket &packet);
  void handlePairResetFrame();
  void handleServoControlFrame(const PresencePacket &packet);
  void handleServoControlBatchFrame(const PresencePacket &packet);
  void handlePairAckFrame(const uint8_t *mac);

  bool addBroadcastPeer();
  bool addPeer(const uint8_t mac[6]);
  bool hasPairedLeader() const;
  bool enqueueServoControl(uint8_t op, uint32_t value, uint16_t requestId, uint8_t sequence);
  bool dequeueServoControl(uint8_t &op, uint32_t &value, uint16_t &requestId, uint8_t &sequence);
  bool isDuplicateControlFrame(uint8_t op, uint32_t value, uint16_t requestId, uint8_t sequence) const;
  void sendCommandAck(uint16_t requestId, uint8_t op, uint8_t status, uint8_t sequence);
  void sendPairRequest(const char *localIp);
  void sendPresence(const char *localIp);

  bool servoScanRequested_{false};
  uint16_t pendingServoScanRequestId_{0U};
  uint8_t pendingServoScanSequence_{0U};
  bool hasLastProcessedScan_{false};
  uint16_t lastProcessedScanRequestId_{0U};
  uint8_t lastProcessedScanSequence_{0U};
  PendingServoControl controlQueue_[config::follower::kServoControlQueueCapacity]{};
  uint8_t controlQueueHead_{0U};
  uint8_t controlQueueTail_{0U};
  uint8_t controlQueueCount_{0U};
  uint8_t lastConsumedControlSequence_{0U};
  bool hasLastProcessedControl_{false};
  uint8_t lastProcessedOp_{0U};
  uint32_t lastProcessedValue_{0U};
  uint16_t lastProcessedRequestId_{0U};
  uint8_t lastProcessedSequence_{0U};
  uint16_t lastAckRequestId_{0U};
  uint8_t lastAckCommandOp_{0U};
  uint8_t lastAckStatus_{0U};
  bool teleopBatchPending_{false};
  uint8_t teleopBatchCount_{0U};
  uint8_t teleopBatchSpeedPct_{0U};
  uint16_t teleopBatchRequestId_{0U};
  uint8_t teleopBatchIds_[config::common::kTeleopBatchMaxServos]{};
  int16_t teleopBatchPositions_[config::common::kTeleopBatchMaxServos]{};
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
