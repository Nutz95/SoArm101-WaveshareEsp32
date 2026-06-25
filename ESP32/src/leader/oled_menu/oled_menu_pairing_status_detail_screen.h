#pragma once

#include "oled_menu_screen_base.h"

namespace soarm {

// Read-only ESP-NOW pairing status (Phase 1; reset comes in Phase 3).
class OledMenuPairingStatusDetailScreen : public OledMenuScreenBase {
public:
  void render(const OledMenuContext &context, OledMenuRenderOutput &out) const override;
};

} // namespace soarm
