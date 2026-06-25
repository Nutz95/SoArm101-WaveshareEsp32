#pragma once

#include "oled_menu_screen_base.h"

namespace soarm {

// Placeholder until IK teleop firmware exists (Phase 5).
class OledMenuIkNotImplementedScreen : public OledMenuScreenBase {
public:
  void render(const OledMenuContext &context, OledMenuRenderOutput &out) const override;
};

} // namespace soarm
