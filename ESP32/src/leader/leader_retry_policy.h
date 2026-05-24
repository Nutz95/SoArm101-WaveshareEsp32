#pragma once

#include <cstdint>

namespace soarm {

uint32_t computeFollowerAckDeadlineMs(
    uint32_t nowMs,
    uint32_t baseTimeoutMs,
    uint8_t retryCount,
    uint32_t retryIntervalMs,
    uint32_t slackMs);

} // namespace soarm
