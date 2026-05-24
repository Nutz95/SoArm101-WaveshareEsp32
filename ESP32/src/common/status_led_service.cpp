#include "status_led_service.h"

#include "../Config/common_runtime_config.h"

#include <Arduino.h>

namespace {

using soarm::ArmRuntimeState;

struct LedStateStyle {
  ArmRuntimeState state;
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  bool blink;
};

// Each entry maps an ArmRuntimeState to an RGB colour and blink policy.
// The state field is the source of truth: order does not matter.
constexpr LedStateStyle kStateStyles[] = {
    {ArmRuntimeState::PairingOrUnpaired,   0U,  0U, 40U, true },
    {ArmRuntimeState::Paired,              0U,  0U, 60U, false},
    {ArmRuntimeState::WaitingCalibration, 60U,  0U,  0U, true },
    {ArmRuntimeState::WaitingEspNow,       0U, 40U,  0U, true },
    {ArmRuntimeState::Ready,               0U, 60U,  0U, false},
    {ArmRuntimeState::ServoFault,         70U,  0U,  0U, true },
};

constexpr LedStateStyle kFallbackStyle{ArmRuntimeState::PairingOrUnpaired, 0U, 0U, 0U, false};

LedStateStyle styleForState(ArmRuntimeState state) {
  for (const LedStateStyle &entry : kStateStyles) {
    if (entry.state == state) {
      return entry;
    }
  }
  return kFallbackStyle;
}

} // namespace

namespace soarm {

StatusLedService::StatusLedService(uint8_t dataPin, uint16_t ledCount)
    : pixels_(ledCount, dataPin, NEO_GRB + NEO_KHZ800) {
}

void StatusLedService::begin() {
  pixels_.begin();
  pixels_.clear();
  pixels_.show();
}

void StatusLedService::render(uint16_t ledIndex, ArmRuntimeState state) {
  if (ledIndex >= pixels_.numPixels()) {
    return;
  }

  const LedStateStyle style = styleForState(state);
  const bool blinkOn = isBlinkOn();
  const bool ledOn = !style.blink || blinkOn;
  const uint32_t color = ledOn ? makeColor(style.red, style.green, style.blue) : makeColor(0U, 0U, 0U);

  pixels_.setPixelColor(ledIndex, color);
  pixels_.show();
}

uint32_t StatusLedService::makeColor(uint8_t red, uint8_t green, uint8_t blue) const {
  return pixels_.Color(red, green, blue);
}

bool StatusLedService::isBlinkOn() const {
  const uint32_t tick = millis() / config::common::kStatusLedBlinkPeriodMs;
  return (tick % 2U) == 0U;
}

} // namespace soarm
