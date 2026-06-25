#include "leader_xbox_controller_service.h"

#include "../Config/controller_mapping_config.h"

#include <array>

namespace soarm {

namespace {

constexpr uint16_t kLogicalButtonA = 1U << 0;
constexpr uint16_t kLogicalButtonB = 1U << 1;
constexpr uint16_t kLogicalButtonX = 1U << 2;
constexpr uint16_t kLogicalButtonY = 1U << 3;
constexpr uint16_t kLogicalButtonLb = 1U << 4;
constexpr uint16_t kLogicalButtonRb = 1U << 5;
constexpr uint16_t kLogicalButtonView = 1U << 6;
constexpr uint16_t kLogicalButtonMenu = 1U << 7;
constexpr uint16_t kLogicalButtonShare = 1U << 8;
constexpr uint16_t kLogicalButtonLeftStick = 1U << 9;
constexpr uint16_t kLogicalButtonRightStick = 1U << 10;
constexpr uint16_t kLogicalDpadUp = 1U << 11;
constexpr uint16_t kLogicalDpadDown = 1U << 12;
constexpr uint16_t kLogicalDpadLeft = 1U << 13;
constexpr uint16_t kLogicalDpadRight = 1U << 14;

struct HatAxes {
  int16_t x;
  int16_t y;
};

struct ButtonBitBinding {
  uint8_t reportByte;
  uint8_t reportBitMask;
  uint16_t logicalMask;
};

// HID hat values are reported by the controller as a compact code.
// This profile uses 0 for neutral and clockwise directions from 1..8.
// 1=Up, 2=UpRight, 3=Right, 4=DownRight, 5=Down, 6=DownLeft, 7=Left, 8=UpLeft.
enum class XboxHatCode : uint8_t {
  Neutral = 0,
  Up = 1,
  UpRight = 2,
  Right = 3,
  DownRight = 4,
  Down = 5,
  DownLeft = 6,
  Left = 7,
  UpLeft = 8,
};

constexpr std::array<ButtonBitBinding, 11> kButtonBitBindings = {{
    {13U, 0x01U, kLogicalButtonA},
    {13U, 0x02U, kLogicalButtonB},
    {13U, 0x08U, kLogicalButtonX},
    {13U, 0x10U, kLogicalButtonY},
    {13U, 0x40U, kLogicalButtonLb},
    {13U, 0x80U, kLogicalButtonRb},
    {14U, 0x01U, kLogicalButtonLeftStick},
    {14U, 0x02U, kLogicalButtonRightStick},
    {14U, 0x04U, kLogicalButtonView},
    {14U, 0x08U, kLogicalButtonMenu},
    {14U, 0x10U, kLogicalButtonShare},
}};

constexpr std::array<uint16_t, 9> kHatMaskByCode = {{
    0U,
    kLogicalDpadUp,
    static_cast<uint16_t>(kLogicalDpadUp | kLogicalDpadRight),
    kLogicalDpadRight,
    static_cast<uint16_t>(kLogicalDpadDown | kLogicalDpadRight),
    kLogicalDpadDown,
    static_cast<uint16_t>(kLogicalDpadDown | kLogicalDpadLeft),
    kLogicalDpadLeft,
    static_cast<uint16_t>(kLogicalDpadUp | kLogicalDpadLeft),
}};

constexpr std::array<HatAxes, 9> kHatAxesByCode = {{
    {0, 0},
    {0, -1},
    {1, -1},
    {1, 0},
    {1, 1},
    {0, 1},
    {-1, 1},
    {-1, 0},
    {-1, -1},
}};

bool buttonPressedFromMask(uint16_t buttonsMask, XboxLogicalButton button) {
  switch (button) {
  case XboxLogicalButton::View:
    return (buttonsMask & kLogicalButtonView) != 0U;
  case XboxLogicalButton::Menu:
    return (buttonsMask & kLogicalButtonMenu) != 0U;
  case XboxLogicalButton::Share:
    return (buttonsMask & kLogicalButtonShare) != 0U;
  case XboxLogicalButton::LeftStick:
    return (buttonsMask & kLogicalButtonLeftStick) != 0U;
  case XboxLogicalButton::RightStick:
    return (buttonsMask & kLogicalButtonRightStick) != 0U;
  case XboxLogicalButton::A:
    return (buttonsMask & kLogicalButtonA) != 0U;
  case XboxLogicalButton::B:
    return (buttonsMask & kLogicalButtonB) != 0U;
  case XboxLogicalButton::None:
  default:
    return false;
  }
}

uint16_t buildLogicalButtonsMask(const uint8_t *data) {
  uint16_t mask = 0U;

  for (const ButtonBitBinding &binding : kButtonBitBindings) {
    if ((data[binding.reportByte] & binding.reportBitMask) != 0U) {
      mask |= binding.logicalMask;
    }
  }

  const uint8_t hatCode = static_cast<uint8_t>(data[12] & 0x0FU);
  if (hatCode < kHatMaskByCode.size()) {
    mask |= kHatMaskByCode[hatCode];
  }

  return mask;
}

void decodeHatToAxes(uint8_t hatCode, int16_t &dpadX, int16_t &dpadY) {
  dpadX = 0;
  dpadY = 0;
  if (hatCode >= kHatAxesByCode.size()) {
    return;
  }

  dpadX = kHatAxesByCode[hatCode].x;
  dpadY = kHatAxesByCode[hatCode].y;
}

int16_t decodeCenteredAxis(const uint8_t *data, uint8_t lowOffset) {
  const uint16_t raw = static_cast<uint16_t>(data[lowOffset]) |
                       (static_cast<uint16_t>(data[static_cast<uint8_t>(lowOffset + 1U)]) << 8U);
  const int32_t centered = static_cast<int32_t>(raw) - 32768;
  return static_cast<int16_t>(centered);
}

void registerButtonPressEdge(bool pressed, std::atomic<bool> &pressedLast, std::atomic<uint8_t> &pendingCount) {
  const bool wasPressed = pressedLast.load();
  if (pressed && !wasPressed) {
    uint8_t pending = pendingCount.load();
    if (pending < 255U) {
      pendingCount.store(static_cast<uint8_t>(pending + 1U));
    }
  }
  pressedLast.store(pressed);
}

} // namespace

void LeaderXboxControllerService::updateInputState(const uint8_t *data, size_t length) {
  if (data == nullptr || length < config::controller::kMinHidReportLength) {
    return;
  }

  const uint16_t buttonsMask = buildLogicalButtonsMask(data);
  buttonsMask_.store(buttonsMask);

  const int16_t leftX = decodeCenteredAxis(data, 0U);
  const int16_t leftY = decodeCenteredAxis(data, 2U);
  const int16_t rightX = decodeCenteredAxis(data, 4U);
  const int16_t rightY = decodeCenteredAxis(data, 6U);

  int16_t dpadX = 0;
  int16_t dpadY = 0;
  decodeHatToAxes(static_cast<uint8_t>(data[12] & 0x0FU), dpadX, dpadY);

  axisLeftX_.store(leftX);
  axisLeftY_.store(leftY);
  axisRightX_.store(rightX);
  axisRightY_.store(rightY);
  dpadX_.store(dpadX);
  dpadY_.store(dpadY);

  const int16_t previousDpadY = dpadYLast_.load();
  if (dpadY < 0 && previousDpadY >= 0) {
    dpadVerticalEdge_.store(1);
  } else if (dpadY > 0 && previousDpadY <= 0) {
    dpadVerticalEdge_.store(-1);
  }
  dpadYLast_.store(dpadY);

  triggerLeft_.store(data[8]);
  triggerRight_.store(data[10]);

  const XboxLogicalButton modeCycle = static_cast<XboxLogicalButton>(modeCycleButton_.load());
  registerButtonPressEdge(
      buttonPressedFromMask(buttonsMask, modeCycle),
      modeCyclePressedLast_,
      modeCyclePendingCount_);

  registerButtonPressEdge(
      buttonPressedFromMask(buttonsMask, XboxLogicalButton::A),
      buttonAPressedLast_,
      buttonAPendingCount_);

  registerButtonPressEdge(
      buttonPressedFromMask(buttonsMask, XboxLogicalButton::B),
      buttonBPressedLast_,
      buttonBPendingCount_);
}

} // namespace soarm
