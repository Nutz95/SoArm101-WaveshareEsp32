#include "leader_app.h"

#include "../Config/leader_runtime_config.h"
#include "../common/servo/servo_control_opcode.h"

namespace soarm {

namespace {

constexpr uint8_t kProfileCalibrationLeader = 0U;
constexpr uint8_t kProfileCalibrationFollower = 1U;
constexpr uint8_t kProfileTeleopEspNow = 2U;
constexpr uint8_t kProfileTeleopWifi = 3U;

} // namespace

void LeaderApp::handleControllerModeCycleEvents() {
  if (xboxControllerService_.consumeModeCycleRequest()) {
    const uint8_t current = controllerOperationProfile_.load();
    const uint8_t next = static_cast<uint8_t>((current + 1U) % 4U);
    applyControllerOperationProfile(next);
    if (next == kProfileCalibrationLeader) {
      setTransientStatus("xbox profile cal leader", config::leader::kMoveStatusHoldMs);
    } else if (next == kProfileCalibrationFollower) {
      setTransientStatus("xbox profile cal follower", config::leader::kMoveStatusHoldMs);
    } else if (next == kProfileTeleopEspNow) {
      setTransientStatus("xbox profile teleop espnow", config::leader::kMoveStatusHoldMs);
    } else {
      setTransientStatus("xbox profile teleop wifi", config::leader::kMoveStatusHoldMs);
    }
  }

  const bool confirmPressed = xboxControllerService_.consumeButtonPress(XboxLogicalButton::A);
  const bool validatePressed = xboxControllerService_.consumeButtonPress(XboxLogicalButton::B);
  const uint8_t profile = controllerOperationProfile_.load();

  if (profile <= kProfileCalibrationFollower) {
    if (confirmPressed && calibrationPhase_.load() == 0U) {
      beginCalibrationRangeCapture();
      setTransientStatus(
          profile == kProfileCalibrationLeader ? "cal leader move extremes" : "cal follower move extremes",
          config::leader::kMoveStatusHoldMs);
    }

    if (confirmPressed && calibrationPhase_.load() != 0U) {
      const bool committed = commitCalibrationRangeCapture();
      if (committed) {
        applyControllerOperationProfile(kProfileTeleopEspNow);
        setTransientStatus("calibration validated", config::leader::kMoveStatusHoldMs);
      } else {
        setTransientStatus("cal telemetry missing", config::leader::kMoveStatusHoldMs);
      }
    }

    if (validatePressed) {
      cancelCalibrationRangeCapture();
      setTransientStatus("calibration canceled", config::leader::kMoveStatusHoldMs);
    }
    return;
  }

  if (confirmPressed) {
    teleopContinuousServoIdFilter_.store(0U);
    teleopContinuousEnabled_.store(true);
    setTransientStatus("teleop mirror start", config::leader::kMoveStatusHoldMs);
  }

  if (validatePressed) {
    teleopContinuousEnabled_.store(false);
    teleopContinuousServoIdFilter_.store(0U);
    if (presenceService_->isFollowerLinked()) {
      const uint16_t requestId = static_cast<uint16_t>(teleopContinuousRequestCounter_ + 1U);
      teleopContinuousRequestCounter_ = requestId;
      (void)presenceService_->requestServoControl(
          static_cast<uint8_t>(ServoControlOpcode::DebugDisable),
          0U,
          requestId);
    }
    setTransientStatus("teleop mirror stop", config::leader::kMoveStatusHoldMs);
  }
}

void LeaderApp::applyControllerOperationProfile(uint8_t profile) {
  const uint8_t safeProfile = profile > 3U ? kProfileTeleopEspNow : profile;
  const uint8_t previous = controllerOperationProfile_.load();
  controllerOperationProfile_.store(safeProfile);

  if (safeProfile <= kProfileCalibrationFollower) {
    calibrationPhase_.store(0U);
    teleopContinuousEnabled_.store(false);
    teleopContinuousServoIdFilter_.store(0U);
    servoDebugManual_ = false;
    servoBusService_.setDebugManual(false);
    if (safeProfile == kProfileCalibrationLeader) {
      servoBusService_.setTorqueEnabledForDetectedServos(false);
    } else {
      if (presenceService_->isFollowerLinked()) {
        const uint16_t requestId = static_cast<uint16_t>(teleopContinuousRequestCounter_ + 1U);
        teleopContinuousRequestCounter_ = requestId;
        (void)presenceService_->requestServoControl(
            static_cast<uint8_t>(ServoControlOpcode::DebugDisable),
            0U,
            requestId);
      }
    }
  } else {
    calibrationPhase_.store(0U);
  }

  if (safeProfile == kProfileTeleopWifi) {
    teleopTransportMode_.store(static_cast<uint8_t>(TeleopTransportMode::WifiUdp));
  } else {
    teleopTransportMode_.store(static_cast<uint8_t>(TeleopTransportMode::EspNow));
  }

  if (previous != safeProfile) {
    if (safeProfile == kProfileCalibrationLeader) {
      setTransientStatus("xbox profile cal leader", config::leader::kMoveStatusHoldMs);
    } else if (safeProfile == kProfileCalibrationFollower) {
      setTransientStatus("xbox profile cal follower", config::leader::kMoveStatusHoldMs);
    } else if (safeProfile == kProfileTeleopEspNow) {
      setTransientStatus("xbox profile teleop espnow", config::leader::kMoveStatusHoldMs);
    } else {
      setTransientStatus("xbox profile teleop wifi", config::leader::kMoveStatusHoldMs);
    }
  }
}

} // namespace soarm
