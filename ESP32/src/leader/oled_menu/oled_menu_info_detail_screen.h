#pragma once

#include "oled_menu_screen_base.h"

namespace soarm {

// Read-only leader/follower/network summary (no list, B returns via navigator).
class OledMenuInfoDetailScreen : public OledMenuScreenBase {
public:
  void render(const OledMenuContext &context, OledMenuRenderOutput &out) const override;
};

} // namespace soarm
