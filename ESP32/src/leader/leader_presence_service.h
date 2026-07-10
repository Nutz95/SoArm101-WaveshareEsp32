#pragma once

#include "../common/interfaces/i_leader_presence_service.h"
#include "../common/link/link_heartbeat_manager.h"
#include "../common/mac_address_utils.h"
#include "../common/presence/espnow_presence_base.h"
#include "../common/presence/link_heartbeat_packet.h"
#include "../common/wifi/wifi_direct_offer_packet.h"
#include "../common/presence/presence_packet.h"
#include "../common/teleop/teleop_espnow_turbo_session.h"
#include "../common/teleop/teleop_load_feedback_codec.h"
#include "../common/peer_pairing_store.h"
#include "../common/servo/servo_control_opcode.h"

#include <cstdint>

#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>

namespace soarm {

class LeaderPresenceService : public ILeaderPresenceService, protected EspNowPresenceBase {
public:
  LeaderPresenceService();

  bool begin() override;
  void tick() override;
  void setPairingWatchdogSuspended(bool suspended) override;
  void refreshFollowerLinkGrace() override;
  void notifyPeerLinkActivity() override;
  bool isFollowerLinked() const override;
  bool canCommandPairedFollower() const;
  bool isFollowerAvailable() const override;
  bool hasValidFollowerIp() const override;
  const char *followerIp() const override;
  bool isPaired() const override;
  const char *pairedPeerMac() const override;
  const char *localMac() const override;
  bool resetPairing() override;
  bool requestServoScan(uint16_t requestId) override;
  bool requestServoControl(uint8_t op, uint32_t value, uint16_t requestId) override;
  bool requestTeleopMirrorBatch(
      const uint8_t *ids,
      const int16_t *positions,
      uint8_t count,
      uint8_t speedPct,
      uint16_t requestId,
      bool turbo = false) override;
  const char *followerServoIds() const override;
  const char *followerServoTelemetry() const override;
  uint8_t followerServoCount() const override;
  bool followerServoDebugManual() const override;
  bool followerServoTemperatureAlarm() const override;
  uint16_t followerLastAckRequestId() const override;
  uint8_t followerLastAckCommandOp() const override;
  uint8_t followerLastAckStatus() const override;
  uint32_t followerLastAckMs() const override;

  bool ensureEspNowTransportReady(uint8_t wifiChannel = 0U);
  bool sendWifiDirectOffer(const WifiDirectOfferPacket &packet);
  bool sendWifiDirectSessionEnd();
  const char *followerWifiDirectIp() const;
  void clearFollowerWifiDirectState();
  /// Clears turbo sparse session state; call when mirror teleop starts (Xbox A).
  void resetTurboTeleopSession();
  /// Drains one pending follower load-feedback ESP-NOW frame (non-blocking).
  bool takeTeleopLoadFeedbackRx(uint8_t loads[6], uint16_t &requestId, uint16_t &gripperPresentPos);
  uint32_t teleopLoadFeedbackTimeoutCount() const;

private:
  void onPresenceFrame(const uint8_t *mac, const uint8_t *data, int len) override;

  // Per-message-type handlers dispatched from onPresenceFrame.
  void handlePairRequest(const uint8_t *mac, const PresencePacket &packet);
  void handlePresenceData(const uint8_t *mac, const PresencePacket &packet);
  void handleServoCommandAck(const uint8_t *mac, const PresencePacket &packet);
  void handleLinkHeartbeat(const uint8_t *mac, const LinkHeartbeatPacket &packet);
  void handleWifiDirectAck(const uint8_t *mac, const WifiDirectAckPacket &packet);
  void handleTeleopLoadFeedbackFrame(const uint8_t *mac, const uint8_t *data, size_t len);

  void sendPairAck(const uint8_t mac[6]);
  void sendPairResetTo(const uint8_t mac[6]);
  void sendPairResetBroadcast();
  void sendServoScanBroadcast(uint16_t requestId);
  bool sendServoControl(const uint8_t mac[6], uint8_t op, uint32_t value, uint16_t requestId);
  bool sendServoControlBatch(
      const uint8_t mac[6],
      const uint8_t *ids,
      const int16_t *positions,
      uint8_t count,
      uint8_t speedPct,
      uint16_t requestId,
      bool turbo);
  bool sendServoControlBroadcast(uint8_t op, uint32_t value, uint16_t requestId);
  bool addBroadcastPeer();
  bool isPairedMac(const uint8_t mac[6]) const;

  PeerPairingStore pairingStore_;
  bool started_{false};
  bool pairingWatchdogSuspended_{false};
  bool hasPairedMac_{false};
  uint8_t pairedFollowerMac_[6]{};
  LinkHeartbeatManager linkHeartbeat_{};
  uint8_t followerServoCount_{0};
  bool followerServoDebugManual_{false};
  bool followerServoTemperatureAlarm_{false};
  uint16_t followerLastAckRequestId_{0U};
  uint8_t followerLastAckCommandOp_{0U};
  uint8_t followerLastAckStatus_{0U};
  uint32_t followerLastAckMs_{0U};
  // Scheduled PairReset broadcasts: sends remaining count one-per-tick at 100 ms intervals.
  uint8_t pendingResetBroadcastCount_{0U};
  uint32_t nextResetBroadcastMs_{0U};
  char followerIp_[16]{};
  char followerWifiDirectIp_[16]{};
  uint32_t followerWifiDirectSessionId_{0U};
  char followerServoIdsText_[48]{};
  char followerServoTelemetryText_[96]{};
  char pairedFollowerMacText_[18]{};
  char localMacText_[18]{};
  TeleopEspNowTurboSession turboEncodeSession_{};
  mutable portMUX_TYPE loadFeedbackMux_ = portMUX_INITIALIZER_UNLOCKED;
  uint8_t pendingLoadFeedbackLoads_[6]{};
  uint16_t pendingLoadFeedbackRequestId_{0U};
  uint16_t pendingLoadFeedbackGripperPos_{0U};
  bool pendingLoadFeedbackReady_{false};
  uint32_t teleopLoadFeedbackTimeoutCount_{0U};
};

} // namespace soarm
