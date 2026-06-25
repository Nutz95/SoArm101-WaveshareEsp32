#pragma once

#include "oled_list_scroll_model.h"
#include "oled_menu_navigation_result.h"
#include "oled_menu_screen_base.h"

namespace soarm {

// Reusable scrollable list with a single ">" cursor (four visible lines).
// Subclasses supply labels and map row activation to a navigation result.
class OledMenuListScreenBase : public OledMenuScreenBase {
public:
  // Resets scroll position when the screen is pushed onto the stack.
  void onEnter() override;
  bool onInput(OledMenuInputEvent event) override;
  void render(const OledMenuContext &context, OledMenuRenderOutput &out) const override;

protected:
  virtual const char *const *labels() const = 0;
  virtual uint8_t labelCount() const = 0;
  // When true, ModeDown moves the highlight down (root menu only).
  virtual bool acceptsModeDown() const { return false; }
  virtual OledMenuNavigationResult onItemActivated(uint8_t itemIndex) const = 0;

  OledListScrollModel scroll_{};
};

} // namespace soarm
