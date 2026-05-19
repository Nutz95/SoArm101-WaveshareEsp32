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

bool LeaderTelemetryStreamServer::consumeResetPairingRequested() {
  const bool pending = resetPairingRequested_;
  resetPairingRequested_ = false;
  return pending;
}

bool LeaderTelemetryStreamServer::consumeServoScanRequested() {
  const bool pending = servoScanRequested_;
  servoScanRequested_ = false;
  return pending;
}

bool LeaderTelemetryStreamServer::consumeServoDebugEnableRequested() {
  const bool pending = servoDebugEnableRequested_;
  servoDebugEnableRequested_ = false;
  return pending;
}

bool LeaderTelemetryStreamServer::consumeServoDebugDisableRequested() {
  const bool pending = servoDebugDisableRequested_;
  servoDebugDisableRequested_ = false;
  return pending;
}

bool LeaderTelemetryStreamServer::consumeServoMoveRequested(uint32_t &value) {
  const bool pending = servoMoveRequested_;
  if (!pending) {
    return false;
  }

  value = servoMoveValue_;
  servoMoveRequested_ = false;
  return true;
}

bool LeaderTelemetryStreamServer::consumeServoSetIdRequested(uint32_t &value) {
  const bool pending = servoSetIdRequested_;
  if (!pending) {
    return false;
  }

  value = servoSetIdValue_;
  servoSetIdRequested_ = false;
  return true;
}

bool LeaderTelemetryStreamServer::consumeServoSetModeRequested(uint32_t &value) {
  const bool pending = servoSetModeRequested_;
  if (!pending) {
    return false;
  }

  value = servoSetModeValue_;
  servoSetModeRequested_ = false;
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
    handleAction(action, frame.value);
  }
}

void LeaderTelemetryStreamServer::handleAction(LeaderCommandAction action, uint32_t value) {
  switch (action) {
  case LeaderCommandAction::StartStream:
    streamEnabled_ = true;
    break;
  case LeaderCommandAction::StopStream:
    streamEnabled_ = false;
    break;
  case LeaderCommandAction::Ping:
    client_.write("PONG", 4);
    break;
  case LeaderCommandAction::ResetPairing:
    resetPairingRequested_ = true;
    break;
  case LeaderCommandAction::ServoScan:
    servoScanRequested_ = true;
    break;
  case LeaderCommandAction::ServoDebugEnable:
    servoDebugEnableRequested_ = true;
    break;
  case LeaderCommandAction::ServoDebugDisable:
    servoDebugDisableRequested_ = true;
    break;
  case LeaderCommandAction::ServoMove:
    servoMoveValue_ = value;
    servoMoveRequested_ = true;
    break;
  case LeaderCommandAction::ServoSetId:
    servoSetIdValue_ = value;
    servoSetIdRequested_ = true;
    break;
  case LeaderCommandAction::ServoSetMode:
    servoSetModeValue_ = value;
    servoSetModeRequested_ = true;
    break;
  case LeaderCommandAction::None:
  default:
    break;
  }
}

void LeaderTelemetryStreamServer::streamTelemetryFrame() {
  const LeaderTelemetrySnapshot snapshot = telemetryState_.snapshot();
  const LeaderTelemetrySerializer::Packet packet = serializer_.serialize(snapshot);
  client_.write(reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
}

} // namespace soarm
