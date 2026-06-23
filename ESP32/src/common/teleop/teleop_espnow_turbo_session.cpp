#include "teleop_espnow_turbo_session.h"

#include "../../Config/common_runtime_config.h"

namespace soarm {

void TeleopEspNowTurboSession::reset() {
  for (uint8_t i = 0U; i < teleop_position_pack::kSlotCount; ++i) {
    slots[i] = 0U;
  }
  knownMask = 0U;
  framesSinceKeyframe = 0U;
  lastKeyframeMs = 0U;
  hasBaselineKeyframe = false;
}

void TeleopEspNowTurboSession::mergePayload(const TeleopEspNowBatchPayload &payload) {
  for (uint8_t i = 0U; i < payload.count && i < config::common::kTeleopBatchMaxServos; ++i) {
    const uint8_t id = payload.ids[i];
    if (id == 0U || id > teleop_position_pack::kSlotCount) {
      continue;
    }
    const uint8_t slot = static_cast<uint8_t>(id - 1U);
    slots[slot] = teleop_position_pack::clampStsPosition(payload.positions[i]);
    knownMask = static_cast<uint8_t>(knownMask | (1U << slot));
  }
}

uint8_t TeleopEspNowTurboSession::maskFromPayload(const TeleopEspNowBatchPayload &payload) const {
  uint8_t mask = 0U;
  for (uint8_t i = 0U; i < payload.count && i < config::common::kTeleopBatchMaxServos; ++i) {
    const uint8_t id = payload.ids[i];
    if (id == 0U || id > teleop_position_pack::kSlotCount) {
      continue;
    }
    mask = static_cast<uint8_t>(mask | (1U << static_cast<uint8_t>(id - 1U)));
  }
  return mask;
}

void TeleopEspNowTurboSession::buildApplyPayload(
    uint8_t activeMask,
    uint16_t requestId,
    uint8_t speedPct,
    TeleopEspNowBatchPayload &out) const {
  out.count = 0U;
  out.requestId = requestId;
  out.speedPct = speedPct;
  out.turbo = true;

  for (uint8_t slot = 0U; slot < teleop_position_pack::kSlotCount; ++slot) {
    if ((activeMask & (1U << slot)) == 0U) {
      continue;
    }
    if (out.count >= config::common::kTeleopBatchMaxServos) {
      break;
    }
    out.ids[out.count] = static_cast<uint8_t>(slot + 1U);
    out.positions[out.count] = static_cast<int16_t>(slots[slot]);
    ++out.count;
  }
}

void TeleopEspNowTurboSession::noteFrameSent(bool keyframe, uint32_t nowMs) {
  if (keyframe) {
    hasBaselineKeyframe = true;
    framesSinceKeyframe = 0U;
    lastKeyframeMs = nowMs;
    return;
  }
  ++framesSinceKeyframe;
}

} // namespace soarm
