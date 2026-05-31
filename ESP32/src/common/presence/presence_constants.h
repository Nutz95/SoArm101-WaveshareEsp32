#pragma once

#include <cstdint>

namespace soarm {

constexpr uint8_t kPresenceMagic = 0xA5;
constexpr uint8_t kPresenceVersion = 1;
constexpr uint32_t kPresenceTxPeriodMs = 250U;
constexpr uint32_t kPresenceTimeoutMs = 12000U;

} // namespace soarm
