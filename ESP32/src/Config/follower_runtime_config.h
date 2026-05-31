#pragma once

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
constexpr uint32_t kPresenceTxPeriodTeleopMs = 250U;
constexpr uint32_t kPresenceTxPeriodWifiTeleopMs = 5000U;
constexpr uint32_t kTeleopWifiDrainMaxBatchesPerTick = 16U;
constexpr uint32_t kTeleopApplyTaskPeriodMs = 17U;
constexpr uint32_t kTeleopApplyTaskIdleDelayMs = 1U;
constexpr uint32_t kTeleopTrafficRecentMs = 2500U;
constexpr uint8_t kServoControlQueueCapacity = 8U;
constexpr uint8_t kCommandAckSendBurstCount = 3U;
constexpr uint8_t kTeleopBatchQueueCapacity = 8U;
constexpr uint32_t kServoTelemetryPublishPeriodMs = 20U;

} // namespace follower
} // namespace config
} // namespace soarm
