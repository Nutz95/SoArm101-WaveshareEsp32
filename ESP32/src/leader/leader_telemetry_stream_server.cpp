#include "leader_telemetry_stream_server.h"

namespace soarm {

LeaderTelemetryStreamServer::LeaderTelemetryStreamServer(LeaderTelemetryState &telemetryState)
    : telemetryState_(telemetryState), server_(0) {
}

bool LeaderTelemetryStreamServer::begin(uint16_t port) {
  server_ = WiFiServer(port);
  server_.begin();
  server_.setNoDelay(true);
  started_ = true;
  return true;
}

bool LeaderTelemetryStreamServer::consumeResetPairingRequested(uint16_t &requestId) {
  const bool pending = resetPairingRequested_;
  requestId = resetPairingRequestId_;
  resetPairingRequested_ = false;
  return pending;
}

bool LeaderTelemetryStreamServer::consumeServoScanRequested(uint32_t &value, uint16_t &requestId) {
  const bool pending = servoScanRequested_;
  value = servoScanValue_;
  requestId = servoScanRequestId_;
  servoScanRequested_ = false;
  return pending;
}

bool LeaderTelemetryStreamServer::consumeServoDebugEnableRequested(uint16_t &requestId) {
  const bool pending = servoDebugEnableRequested_;
  requestId = servoDebugEnableRequestId_;
  servoDebugEnableRequested_ = false;
  return pending;
}

bool LeaderTelemetryStreamServer::consumeServoDebugDisableRequested(uint16_t &requestId) {
  const bool pending = servoDebugDisableRequested_;
  requestId = servoDebugDisableRequestId_;
  servoDebugDisableRequested_ = false;
  return pending;
}

bool LeaderTelemetryStreamServer::consumeServoDebugEnableFollowerRequested(uint16_t &requestId) {
  const bool pending = servoDebugEnableFollowerRequested_;
  requestId = servoDebugEnableFollowerRequestId_;
  servoDebugEnableFollowerRequested_ = false;
  return pending;
}

bool LeaderTelemetryStreamServer::consumeServoDebugDisableFollowerRequested(uint16_t &requestId) {
  const bool pending = servoDebugDisableFollowerRequested_;
  requestId = servoDebugDisableFollowerRequestId_;
  servoDebugDisableFollowerRequested_ = false;
  return pending;
}

bool LeaderTelemetryStreamServer::consumeServoMoveRequested(uint32_t &value, uint16_t &requestId) {
  const bool pending = servoMoveRequested_;
  if (!pending) {
    return false;
  }

  value = servoMoveValue_;
  requestId = servoMoveRequestId_;
  servoMoveRequested_ = false;
  return true;
}

bool LeaderTelemetryStreamServer::consumeServoSetIdRequested(uint32_t &value, uint16_t &requestId) {
  const bool pending = servoSetIdRequested_;
  if (!pending) {
    return false;
  }

  value = servoSetIdValue_;
  requestId = servoSetIdRequestId_;
  servoSetIdRequested_ = false;
  return true;
}

bool LeaderTelemetryStreamServer::consumeServoSetModeRequested(uint32_t &value, uint16_t &requestId) {
  const bool pending = servoSetModeRequested_;
  if (!pending) {
    return false;
  }

  value = servoSetModeValue_;
  requestId = servoSetModeRequestId_;
  servoSetModeRequested_ = false;
  return true;
}

bool LeaderTelemetryStreamServer::consumeTeleopMirrorRequested(uint32_t &value, uint16_t &requestId) {
  const bool pending = teleopMirrorRequested_;
  if (!pending) {
    return false;
  }

  value = teleopMirrorValue_;
  requestId = teleopMirrorRequestId_;
  teleopMirrorRequested_ = false;
  return true;
}

bool LeaderTelemetryStreamServer::consumeTeleopContinuousRequested(uint32_t &value, uint16_t &requestId) {
  const bool pending = teleopContinuousRequested_;
  if (!pending) {
    return false;
  }

  value = teleopContinuousValue_;
  requestId = teleopContinuousRequestId_;
  teleopContinuousRequested_ = false;
  return true;
}

void LeaderTelemetryStreamServer::tick() {
  if (!started_) {
    return;
  }

  if (!client_ || !client_.connected()) {
    WiFiClient incoming = server_.available();
    if (incoming) {
      client_.stop();
      client_ = incoming;
      client_.setNoDelay(true);
      streamEnabled_ = false;
    }
    return;
  }

  handleIncomingCommands();

  if (!streamEnabled_) {
    return;
  }

  const uint32_t nowMs = millis();
  if ((nowMs - lastStreamMs_) < 50U) {
    return;
  }

  lastStreamMs_ = nowMs;
  streamTelemetryFrame();
}

void LeaderTelemetryStreamServer::handleIncomingCommands() {
  while (client_.available() >= static_cast<int>(sizeof(LeaderCommandProcessor::CommandFrame))) {
    LeaderCommandProcessor::CommandFrame frame{};
    const size_t readLen = client_.readBytes(
        reinterpret_cast<char *>(&frame), sizeof(LeaderCommandProcessor::CommandFrame));

    if (readLen != sizeof(LeaderCommandProcessor::CommandFrame)) {
      return;
    }

    const LeaderCommandAction action = commandProcessor_.process(frame);
    handleAction(action, frame.value, frame.requestId);
  }
}

void LeaderTelemetryStreamServer::handleAction(LeaderCommandAction action, uint32_t value, uint16_t requestId) {
  struct FlagActionEntry {
    LeaderCommandAction action;
    bool LeaderTelemetryStreamServer::*flag;
    uint16_t LeaderTelemetryStreamServer::*requestId;
  };

  static const FlagActionEntry kFlagActions[] = {
      {LeaderCommandAction::ResetPairing, &LeaderTelemetryStreamServer::resetPairingRequested_, &LeaderTelemetryStreamServer::resetPairingRequestId_},
      {LeaderCommandAction::ServoDebugEnable, &LeaderTelemetryStreamServer::servoDebugEnableRequested_, &LeaderTelemetryStreamServer::servoDebugEnableRequestId_},
      {LeaderCommandAction::ServoDebugDisable, &LeaderTelemetryStreamServer::servoDebugDisableRequested_, &LeaderTelemetryStreamServer::servoDebugDisableRequestId_},
      {LeaderCommandAction::ServoDebugEnableFollower, &LeaderTelemetryStreamServer::servoDebugEnableFollowerRequested_, &LeaderTelemetryStreamServer::servoDebugEnableFollowerRequestId_},
      {LeaderCommandAction::ServoDebugDisableFollower, &LeaderTelemetryStreamServer::servoDebugDisableFollowerRequested_, &LeaderTelemetryStreamServer::servoDebugDisableFollowerRequestId_},
  };

  struct ValueActionEntry {
    LeaderCommandAction action;
    bool LeaderTelemetryStreamServer::*flag;
    uint32_t LeaderTelemetryStreamServer::*value;
    uint16_t LeaderTelemetryStreamServer::*requestId;
    uint32_t fixedValue;
    bool useFixedValue;
  };

  static const ValueActionEntry kValueActions[] = {
      {LeaderCommandAction::ServoScan, &LeaderTelemetryStreamServer::servoScanRequested_, &LeaderTelemetryStreamServer::servoScanValue_, &LeaderTelemetryStreamServer::servoScanRequestId_, 0U, false},
      {LeaderCommandAction::ServoScanLeader, &LeaderTelemetryStreamServer::servoScanRequested_, &LeaderTelemetryStreamServer::servoScanValue_, &LeaderTelemetryStreamServer::servoScanRequestId_, 1U, true},
      {LeaderCommandAction::ServoScanFollower, &LeaderTelemetryStreamServer::servoScanRequested_, &LeaderTelemetryStreamServer::servoScanValue_, &LeaderTelemetryStreamServer::servoScanRequestId_, 2U, true},
      {LeaderCommandAction::ServoMove, &LeaderTelemetryStreamServer::servoMoveRequested_, &LeaderTelemetryStreamServer::servoMoveValue_, &LeaderTelemetryStreamServer::servoMoveRequestId_, 0U, false},
      {LeaderCommandAction::ServoSetId, &LeaderTelemetryStreamServer::servoSetIdRequested_, &LeaderTelemetryStreamServer::servoSetIdValue_, &LeaderTelemetryStreamServer::servoSetIdRequestId_, 0U, false},
      {LeaderCommandAction::ServoSetMode, &LeaderTelemetryStreamServer::servoSetModeRequested_, &LeaderTelemetryStreamServer::servoSetModeValue_, &LeaderTelemetryStreamServer::servoSetModeRequestId_, 0U, false},
      {LeaderCommandAction::TeleopMirror, &LeaderTelemetryStreamServer::teleopMirrorRequested_, &LeaderTelemetryStreamServer::teleopMirrorValue_, &LeaderTelemetryStreamServer::teleopMirrorRequestId_, 0U, false},
      {LeaderCommandAction::TeleopContinuousSet, &LeaderTelemetryStreamServer::teleopContinuousRequested_, &LeaderTelemetryStreamServer::teleopContinuousValue_, &LeaderTelemetryStreamServer::teleopContinuousRequestId_, 0U, false},
  };

  for (size_t i = 0; i < (sizeof(kFlagActions) / sizeof(kFlagActions[0])); ++i) {
    if (kFlagActions[i].action == action) {
      this->*(kFlagActions[i].flag) = true;
      this->*(kFlagActions[i].requestId) = requestId;
      return;
    }
  }

  for (size_t i = 0; i < (sizeof(kValueActions) / sizeof(kValueActions[0])); ++i) {
    if (kValueActions[i].action == action) {
      this->*(kValueActions[i].flag) = true;
      this->*(kValueActions[i].value) = kValueActions[i].useFixedValue ? kValueActions[i].fixedValue : value;
      this->*(kValueActions[i].requestId) = requestId;
      return;
    }
  }

  if (action == LeaderCommandAction::StartStream) {
    streamEnabled_ = true;
    return;
  }

  if (action == LeaderCommandAction::StopStream) {
    streamEnabled_ = false;
    return;
  }

  if (action == LeaderCommandAction::Ping) {
    client_.write("PONG", 4);
  }
}

void LeaderTelemetryStreamServer::streamTelemetryFrame() {
  const LeaderTelemetrySnapshot snapshot = telemetryState_.snapshot();
  const LeaderTelemetrySerializer::Packet packet = serializer_.serialize(snapshot);
  client_.write(reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
}

} // namespace soarm
