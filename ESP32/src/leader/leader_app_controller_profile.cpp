#include "leader_app.h"

#include "../Config/leader_runtime_config.h"
#include "../common/controller/controller_operation_profile.h"
#include "../common/servo/servo_control_opcode.h"
#include "../common/teleop/teleop_transport_mode.h"

namespace soarm {

namespace {

using Profile = ControllerOperationProfile;

constexpr Profile kProfileCalibrationLeader = Profile::CalibrationLeader;
constexpr Profile kProfileCalibrationFollower = Profile::CalibrationFollower;
constexpr Profile kProfileTeleopEspNow = Profile::TeleopEspNow;
constexpr Profile kProfileTeleopWifi = Profile::TeleopWifi;
constexpr Profile kProfileTeleopPcSerial = Profile::TeleopPcSerial;
constexpr Profile kProfilePassthrough = Profile::Passthrough;
constexpr Profile kProfileOtaReady = Profile::OtaReady;

} // namespace

void LeaderApp::engagePassthroughMode() {
  const ControllerOperationProfile profile =
      sanitizeControllerOperationProfile(controllerOperationProfile_.load());
  if (profile != ControllerOperationProfile::Passthrough) {
    return;
  }

  passthroughEngaged_.store(true);
  servoPassthrough_.enter(Serial2);
  setTransientStatus("passthrough active", config::leader::kMoveStatusHoldMs);
}

void LeaderApp::disengagePassthroughMode(ControllerOperationProfile fallbackProfile) {
  if (passthroughEngaged_.load()) {
    passthroughEngaged_.store(false);
    servoPassthrough_.exit();
  }

  const ControllerOperationProfile current =
      sanitizeControllerOperationProfile(controllerOperationProfile_.load());
  if (current == ControllerOperationProfile::Passthrough) {
    applyControllerOperationProfile(toProfileRaw(fallbackProfile));
  }
}

void LeaderApp::handleControllerModeCycleEvents() {
  if (xboxControllerService_.consumeModeCycleRequest()) {
    const Profile current = sanitizeControllerOperationProfile(controllerOperationProfile_.load());
    if (current == kProfilePassthrough) {
      disengagePassthroughMode(kProfileTeleopEspNow);
    }
    const uint8_t nextRaw =
        static_cast<uint8_t>((toProfileRaw(current) + 1U) % kControllerOperationProfileCount);
    applyControllerOperationProfile(nextRaw);
    const Profile next = sanitizeControllerOperationProfile(nextRaw);
    if (next == kProfileCalibrationLeader) {
      setTransientStatus("xbox profile cal leader", config::leader::kMoveStatusHoldMs);
    } else if (next == kProfileCalibrationFollower) {
      setTransientStatus("cal follower? press A", config::leader::kMoveStatusHoldMs);
    } else if (next == kProfileTeleopEspNow) {
      setTransientStatus("xbox profile teleop espnow", config::leader::kMoveStatusHoldMs);
    } else if (next == kProfileTeleopWifi) {
      setTransientStatus("xbox profile teleop wifi", config::leader::kMoveStatusHoldMs);
    } else if (next == kProfileTeleopPcSerial) {
      setTransientStatus("xbox profile teleop pc serial", config::leader::kMoveStatusHoldMs);
    } else if (next == kProfilePassthrough) {
      setTransientStatus("passthrough? press A", config::leader::kMoveStatusHoldMs);
    } else if (next == kProfileOtaReady) {
      setTransientStatus("OTA: wifi on, flash now", config::leader::kMoveStatusHoldMs);
    } else {
      setTransientStatus("profile changed", config::leader::kMoveStatusHoldMs);
    }
  }

  const bool confirmPressed = xboxControllerService_.consumeButtonPress(XboxLogicalButton::A);
  const bool validatePressed = xboxControllerService_.consumeButtonPress(XboxLogicalButton::B);
  const Profile profile = sanitizeControllerOperationProfile(controllerOperationProfile_.load());

  if (profile == kProfilePassthrough) {
    if (confirmPressed) {
      engagePassthroughMode();
    }
    if (validatePressed) {
      disengagePassthroughMode(kProfileTeleopEspNow);
      setTransientStatus("passthrough canceled", config::leader::kMoveStatusHoldMs);
    }
    return;
  }

  if (profile == kProfileCalibrationLeader || profile == kProfileCalibrationFollower) {
    if (confirmPressed) {
      if (followerCalibrationCenterPending_.load()) {
        return;
      }
      if (calibrationPhase_.load() == 0U) {
        if (beginCalibrationRangeCapture()) {
          if (profile == kProfileCalibrationFollower) {
            setTransientStatus("cal follower center", config::leader::kMoveStatusHoldMs);
          } else {
            setTransientStatus("cal leader move extremes", config::leader::kMoveStatusHoldMs);
          }
        } else {
          setTransientStatus("cal center failed", config::leader::kMoveStatusHoldMs);
        }
      } else {
        const bool committed = commitCalibrationRangeCapture();
        if (committed) {
          applyControllerOperationProfile(toProfileRaw(kProfileTeleopEspNow));
          setTransientStatus("calibration validated", config::leader::kMoveStatusHoldMs);
        } else {
          setTransientStatus("cal telemetry missing", config::leader::kMoveStatusHoldMs);
        }
      }
    }

    if (validatePressed) {
      cancelCalibrationRangeCapture();
      applyControllerOperationProfile(toProfileRaw(kProfileTeleopEspNow));
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

void LeaderApp::applyControllerOperationProfile(uint8_t profileRaw) {
  const Profile safeProfile = sanitizeControllerOperationProfile(profileRaw);
  const Profile previous = sanitizeControllerOperationProfile(controllerOperationProfile_.load());
  controllerOperationProfile_.store(toProfileRaw(safeProfile));

  if (previous == kProfilePassthrough && passthroughEngaged_.load()) {
    passthroughEngaged_.store(false);
    servoPassthrough_.exit();
  }

  if (safeProfile == kProfilePassthrough) {
    calibrationPhase_.store(0U);
    teleopContinuousEnabled_.store(false);
    teleopContinuousServoIdFilter_.store(0U);
    return;
  }

  if (safeProfile == kProfileCalibrationLeader || safeProfile == kProfileCalibrationFollower) {
    calibrationPhase_.store(0U);
    followerCalibrationCenterPending_.store(false);
    teleopContinuousEnabled_.store(false);
    teleopContinuousServoIdFilter_.store(0U);
    servoDebugManual_ = false;
    servoBusService_.setDebugManual(false);
    if (safeProfile == kProfileCalibrationLeader) {
      servoBusService_.setTorqueEnabledForDetectedServos(false);
    }
  } else {
    calibrationPhase_.store(0U);
  }

  if (safeProfile == kProfileTeleopWifi) {
    teleopTransportMode_.store(static_cast<uint8_t>(TeleopTransportMode::WifiUdp));
  } else if (safeProfile == kProfileTeleopPcSerial) {
    teleopTransportMode_.store(static_cast<uint8_t>(TeleopTransportMode::PcSerialBridge));
  } else if (safeProfile == kProfileTeleopEspNow) {
    teleopTransportMode_.store(static_cast<uint8_t>(TeleopTransportMode::EspNow));
  }

  if ((previous == kProfileCalibrationLeader || previous == kProfileCalibrationFollower) &&
      (safeProfile == kProfileTeleopEspNow || safeProfile == kProfileTeleopWifi ||
       safeProfile == kProfileTeleopPcSerial)) {
    nudgeFollowerLinkAfterCalibration();
  }

  syncWifiRadioPolicyForProfile(safeProfile);

  if (previous != safeProfile) {
    if (safeProfile == kProfileCalibrationLeader) {
      setTransientStatus("xbox profile cal leader", config::leader::kMoveStatusHoldMs);
    } else if (safeProfile == kProfileCalibrationFollower) {
      setTransientStatus("cal follower? press A", config::leader::kMoveStatusHoldMs);
    } else if (safeProfile == kProfileTeleopEspNow) {
      setTransientStatus("xbox profile teleop espnow", config::leader::kMoveStatusHoldMs);
    } else if (safeProfile == kProfileTeleopWifi) {
      setTransientStatus("xbox profile teleop wifi", config::leader::kMoveStatusHoldMs);
    } else if (safeProfile == kProfileTeleopPcSerial) {
      setTransientStatus("pc serial: start COM bridge", config::leader::kMoveStatusHoldMs);
    } else if (safeProfile == kProfileOtaReady) {
      setTransientStatus("OTA: wifi on, flash now", config::leader::kMoveStatusHoldMs);
    }
  }
}

void LeaderApp::nudgeFollowerLinkAfterCalibration() {
  presenceService_->refreshFollowerLinkGrace();
  if (!presenceService_->isFollowerAvailable()) {
    return;
  }

  followerStartupScanDone_ = false;
  followerStartupScanPending_ = false;
  followerStartupScanRequestId_ = static_cast<uint16_t>(followerStartupScanRequestId_ + 1U);
  (void)presenceService_->requestServoScan(followerStartupScanRequestId_);

  const uint16_t requestId = static_cast<uint16_t>(teleopContinuousRequestCounter_ + 1U);
  teleopContinuousRequestCounter_ = requestId;
  (void)presenceService_->requestServoControl(
      static_cast<uint8_t>(ServoControlOpcode::DebugDisable),
      0U,
      requestId);
}

} // namespace soarm
