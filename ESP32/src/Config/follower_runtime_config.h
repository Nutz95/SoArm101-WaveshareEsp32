#pragma once

#include <cstdint>

namespace soarm {
namespace config {
namespace follower {

constexpr uint32_t kTickDelayMs = 10U;
constexpr uint32_t kCalibrationReadyMs = 5000U;
constexpr uint32_t kEspNowLinkedReadyMs = 8000U;
constexpr uint16_t kTeleopServoMaxSpeedRaw = 7000U;
constexpr uint32_t kPairRequestIntervalMs = 5000U;
constexpr uint8_t kServoControlQueueCapacity = 8U;
constexpr uint8_t kCommandAckSendBurstCount = 2U;

} // namespace follower
} // namespace config
} // namespace soarm
