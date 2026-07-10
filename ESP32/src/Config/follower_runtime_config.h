#pragma once

#include "common_runtime_config.h"

#include <cstdint>

namespace soarm {
namespace config {
namespace follower {

constexpr uint32_t kTickDelayMs = 2U;
constexpr uint32_t kCalibrationReadyMs = 5000U;
constexpr uint32_t kEspNowLinkedReadyMs = 8000U;
constexpr uint16_t kTeleopServoMaxSpeedRaw = 7000U;
constexpr uint32_t kPairRequestIntervalMs = 5000U;
constexpr uint32_t kPairRequestIntervalTeleopMs = 30000U;
constexpr uint32_t kTeleopWifiDrainMaxBatchesPerTick = 16U;
constexpr uint32_t kTeleopLoadSamplerPeriodMs = config::common::kTeleopTurboControlPeriodMs;
constexpr uint32_t kTeleopLoadSamplerPhaseOffsetMs = 5U;
/// Present load is only trusted when |present speed| is below this (STS units).
constexpr int16_t kTeleopLoadSampleMaxAbsSpeed = 80;
constexpr uint32_t kTeleopApplyTaskPeriodMs = config::common::kTeleopControlPeriodMs;
constexpr uint32_t kTeleopTurboApplyTaskPeriodMs = config::common::kTeleopTurboControlPeriodMs;
constexpr uint32_t kTeleopApplyTaskIdleDelayMs = 1U;
constexpr uint32_t kTeleopTrafficRecentMs = 8000U;
constexpr uint32_t kWifiDirectStaConnectTimeoutMs = 20000U;
constexpr uint32_t kPostWifiDirectEspNowResyncTimeoutMs = 3000U;
constexpr uint8_t kServoControlQueueCapacity = 8U;
constexpr uint8_t kCommandAckSendBurstCount = 3U;
constexpr uint8_t kTeleopBatchQueueCapacity = 8U;
constexpr uint32_t kServoTelemetryPublishPeriodMs = 20U;
constexpr uint32_t kCalibrationTelemetryBoostMs = 120000U;
// Full presence cadence when idle/calibration (not ESP-NOW teleop batches).
constexpr uint32_t kIdleFullPresenceIntervalMs = 500U;
constexpr uint32_t kServoCommandPresenceBoostMs = 5000U;

} // namespace follower
} // namespace config
} // namespace soarm
