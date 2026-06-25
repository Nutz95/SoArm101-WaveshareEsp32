#include "follower_app.h"

#include "../Config/common_runtime_config.h"
#include "../Config/follower_runtime_config.h"
#include "../common/command/command_ack_status.h"
#include "../common/teleop/teleop_packet_flags.h"
#include "follower_presence_service.h"

#include <Arduino.h>

namespace soarm {

namespace {

uint8_t normalizeTeleopBatch(
    const uint8_t *idsIn, const int16_t *posIn, uint8_t countIn,
    uint8_t *idsOut, int16_t *posOut, uint8_t maxOut,
    const char *detectedText) {
  (void)detectedText;
  uint8_t countOut = 0U;
  for (uint8_t i = 0U; i < countIn && countOut < maxOut; ++i) {
    if (idsIn[i] == 0U) {
      continue;
    }
    idsOut[countOut] = idsIn[i];
    posOut[countOut] = posIn[i];
    ++countOut;
  }
  return countOut;
}

} // namespace

void FollowerApp::markTeleopActivity(uint32_t nowMs) {
  lastTeleopActivityMs_ = nowMs;
}

bool FollowerApp::isTeleopBusPaused(uint32_t nowMs) const {
  if (calibrationTelemetryBoostUntilMs_ > 0U && nowMs < calibrationTelemetryBoostUntilMs_) {
    return false;
  }
  return lastTeleopActivityMs_ > 0U &&
         (nowMs - lastTeleopActivityMs_) < config::follower::kTeleopTrafficRecentMs;
}

bool FollowerApp::ensureTeleopServosReady(const uint8_t *ids, uint8_t count) {
  if (ids == nullptr) {
    return false;
  }

  bool hasCandidate = false;

  for (uint8_t i = 0U; i < count; ++i) {
    const uint8_t id = ids[i];
    if (id == 0U) {
      continue;
    }

    hasCandidate = true;
    if (teleopPreparedById_[id]) {
      continue;
    }
    const bool modeOk = servoBusService_.setServoMode(id, 0U);
    const bool torqueOk = servoBusService_.setTorqueEnabled(id, true);
    if (!(modeOk && torqueOk)) {
      Serial.printf("[SERVO] teleop prepare fail id=%u\n", id);
      continue;
    }
    teleopPreparedById_[id] = true;
  }

  return hasCandidate;
}

bool FollowerApp::applyOneTeleopWifiBatch(
    uint8_t *ids,
    int16_t *positions,
    uint8_t count,
    uint8_t speedPercent,
    uint16_t requestId,
    uint8_t flags) {
  const uint16_t speed = static_cast<uint16_t>(
      (static_cast<uint32_t>(speedPercent) * config::follower::kTeleopServoMaxSpeedRaw) / 100U);
  uint8_t filteredIds[config::common::kTeleopBatchMaxServos]{};
  int16_t filteredPositions[config::common::kTeleopBatchMaxServos]{};
  const uint8_t filteredCount = normalizeTeleopBatch(
      ids, positions, count,
      filteredIds, filteredPositions, config::common::kTeleopBatchMaxServos,
      servoBusService_.lastIdsText());
  const bool requireAck = (flags & teleop::kFlagRequireAck) != 0U;
  if (filteredCount == 0U) {
    if (requireAck) {
      teleopWifiBridge_.sendAck(requestId, static_cast<uint8_t>(CommandAckStatus::Applied));
    }
    return false;
  }
  if (!ensureTeleopServosReady(filteredIds, filteredCount)) {
    if (requireAck) {
      teleopWifiBridge_.sendAck(requestId, static_cast<uint8_t>(CommandAckStatus::Failed));
    }
    return false;
  }
  const bool ok = servoBusService_.moveBatch(filteredIds, filteredPositions, filteredCount, speed, false);
  if (requireAck) {
    teleopWifiBridge_.sendAck(requestId, static_cast<uint8_t>(ok ? CommandAckStatus::Applied : CommandAckStatus::Failed));
  }
  presenceService_->notifyWifiTeleopActivity();
  return ok;
}

bool FollowerApp::applyTeleopBatch(
    const uint8_t *ids,
    const int16_t *positions,
    uint8_t count,
    uint8_t speedPercent,
    uint16_t requestId,
    uint8_t flags,
    bool stageEspNowAck) {
  uint8_t mutableIds[config::common::kTeleopBatchMaxServos]{};
  int16_t mutablePositions[config::common::kTeleopBatchMaxServos]{};
  for (uint8_t i = 0U; i < count && i < config::common::kTeleopBatchMaxServos; ++i) {
    mutableIds[i] = ids[i];
    mutablePositions[i] = positions[i];
  }

  if (stageEspNowAck) {
    const uint16_t speed = static_cast<uint16_t>(
        (static_cast<uint32_t>(speedPercent) * config::follower::kTeleopServoMaxSpeedRaw) / 100U);
    uint8_t filteredIds[config::common::kTeleopBatchMaxServos]{};
    int16_t filteredPositions[config::common::kTeleopBatchMaxServos]{};
    const uint8_t filteredCount = normalizeTeleopBatch(
        mutableIds, mutablePositions, count,
        filteredIds, filteredPositions, config::common::kTeleopBatchMaxServos,
        servoBusService_.lastIdsText());
    if (filteredCount == 0U) {
      presenceService_->stageTeleopBatchAck(requestId, static_cast<uint8_t>(CommandAckStatus::Applied));
      return false;
    }
    if (!ensureTeleopServosReady(filteredIds, filteredCount)) {
      presenceService_->stageTeleopBatchAck(requestId, static_cast<uint8_t>(CommandAckStatus::Failed));
      return false;
    }
    const bool ok = servoBusService_.moveBatch(filteredIds, filteredPositions, filteredCount, speed, false);
    presenceService_->stageTeleopBatchAck(
        requestId,
        static_cast<uint8_t>(ok ? CommandAckStatus::Applied : CommandAckStatus::Failed));
    if (ok) {
      sendTeleopLoadFeedbackAfterApply(requestId);
    }
    return ok;
  }

  return applyOneTeleopWifiBatch(
      mutableIds, mutablePositions, count, speedPercent, requestId, flags);
}

void FollowerApp::sendTeleopLoadFeedbackAfterApply(uint16_t requestId) {
  auto *presence = static_cast<FollowerPresenceService *>(presenceService_.get());
  if (presence == nullptr || !presence->isTeleopLoadFeedbackUplinkEnabled()) {
    return;
  }

  uint8_t loads[config::common::kTeleopBatchMaxServos]{};
  teleopLoadSnapshot_.copyLoads(loads);
  presence->sendTeleopLoadFeedback(requestId, loads);
}

} // namespace soarm
