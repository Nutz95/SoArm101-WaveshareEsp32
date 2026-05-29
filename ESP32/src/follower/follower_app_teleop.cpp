#include "follower_app.h"

#include "../Config/common_runtime_config.h"
#include "../Config/follower_runtime_config.h"
#include "../common/command/command_ack_status.h"
#include "../common/servo/servo_control_opcode.h"

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

bool FollowerApp::ensureTeleopServosReady(const uint8_t *ids, uint8_t count) {
  if (ids == nullptr) {
    return false;
  }

  for (uint8_t i = 0U; i < count; ++i) {
    const uint8_t id = ids[i];
    if (id == 0U || teleopPreparedById_[id]) {
      continue;
    }
    if (!servoBusService_.setServoMode(id, 0U) || !servoBusService_.setTorqueEnabled(id, true)) {
      Serial.printf("[SERVO] teleop prepare fail id=%u\n", id);
      return false;
    }
    teleopPreparedById_[id] = true;
  }

  return true;
}

void FollowerApp::processIncomingTeleopWifiBatch() {
  uint8_t ids[config::common::kTeleopBatchMaxServos]{};
  int16_t positions[config::common::kTeleopBatchMaxServos]{};
  uint8_t count = 0U;
  uint8_t speedPercent = 0U;
  uint16_t requestId = 0U;

  if (!teleopWifiBridge_.consumeBatch(ids, positions, config::common::kTeleopBatchMaxServos, count, speedPercent, requestId)) {
    return;
  }

  const uint16_t speed = static_cast<uint16_t>(
      (static_cast<uint32_t>(speedPercent) * config::follower::kTeleopServoMaxSpeedRaw) / 100U);
  uint8_t filteredIds[config::common::kTeleopBatchMaxServos]{};
  int16_t filteredPositions[config::common::kTeleopBatchMaxServos]{};
    const uint8_t filteredCount = normalizeTeleopBatch(
      ids, positions, count,
      filteredIds, filteredPositions, config::common::kTeleopBatchMaxServos,
      servoBusService_.lastIdsText());
  if (filteredCount == 0U) {
    teleopWifiBridge_.sendAck(requestId, static_cast<uint8_t>(CommandAckStatus::Applied));
    return;
  }
  if (!ensureTeleopServosReady(filteredIds, filteredCount)) {
    teleopWifiBridge_.sendAck(requestId, static_cast<uint8_t>(CommandAckStatus::Failed));
    return;
  }
  const bool ok = servoBusService_.moveBatch(filteredIds, filteredPositions, filteredCount, speed);
  teleopWifiBridge_.sendAck(requestId, static_cast<uint8_t>(ok ? CommandAckStatus::Applied : CommandAckStatus::Failed));
}

void FollowerApp::processIncomingTeleopBatch() {
  uint8_t ids[config::common::kTeleopBatchMaxServos]{};
  int16_t positions[config::common::kTeleopBatchMaxServos]{};
  uint8_t count = 0U;
  uint8_t speedPercent = 0U;
  uint16_t requestId = 0U;

  if (!presenceService_->consumeTeleopMirrorBatch(
          ids, positions, config::common::kTeleopBatchMaxServos, count, speedPercent, requestId)) {
    return;
  }

  const uint16_t speed = static_cast<uint16_t>(
      (static_cast<uint32_t>(speedPercent) * config::follower::kTeleopServoMaxSpeedRaw) / 100U);
  uint8_t filteredIds[config::common::kTeleopBatchMaxServos]{};
  int16_t filteredPositions[config::common::kTeleopBatchMaxServos]{};
    const uint8_t filteredCount = normalizeTeleopBatch(
      ids, positions, count,
      filteredIds, filteredPositions, config::common::kTeleopBatchMaxServos,
      servoBusService_.lastIdsText());
  if (filteredCount == 0U) {
    presenceService_->updateLastCommandAck(
        requestId,
        static_cast<uint8_t>(ServoControlOpcode::TeleopMirrorBatch),
        static_cast<uint8_t>(CommandAckStatus::Applied));
    presenceService_->requestImmediatePresenceTx();
    return;
  }
  if (!ensureTeleopServosReady(filteredIds, filteredCount)) {
    presenceService_->updateLastCommandAck(
        requestId,
        static_cast<uint8_t>(ServoControlOpcode::TeleopMirrorBatch),
        static_cast<uint8_t>(CommandAckStatus::Failed));
    presenceService_->requestImmediatePresenceTx();
    return;
  }
  const bool ok = servoBusService_.moveBatch(filteredIds, filteredPositions, filteredCount, speed);
  presenceService_->updateLastCommandAck(
      requestId,
      static_cast<uint8_t>(ServoControlOpcode::TeleopMirrorBatch),
      static_cast<uint8_t>(ok ? CommandAckStatus::Applied : CommandAckStatus::Failed));
  presenceService_->requestImmediatePresenceTx();
}

} // namespace soarm