#include "oled_menu_list_screen_base.h"

#include <cstdio>

namespace soarm {

void OledMenuListScreenBase::onEnter() {
  scroll_.reset(labelCount(), 0U);
}

bool OledMenuListScreenBase::onInput(OledMenuInputEvent event) {
  if (event == OledMenuInputEvent::NavigateUp) {
    scroll_.moveUp();
    return true;
  }

  if (event == OledMenuInputEvent::NavigateDown ||
      (event == OledMenuInputEvent::ModeDown && acceptsModeDown())) {
    scroll_.moveDown();
    return true;
  }

  if (event == OledMenuInputEvent::Select) {
    return applyNavigationResult(onItemActivated(scroll_.selectedIndex()));
  }

  return false;
}

void OledMenuListScreenBase::render(const OledMenuContext &context, OledMenuRenderOutput &out) const {
  (void)context;
  clearOutput(out);

  const uint8_t count = labelCount();
  const char *const *itemLabels = labels();
  if (itemLabels == nullptr || count == 0U) {
    return;
  }

  for (uint8_t line = 0U; line < kOledMenuVisibleLines; ++line) {
    const uint8_t itemIndex = scroll_.visibleItemIndex(line);
    if (itemIndex >= count) {
      out.lines[line][0] = '\0';
      continue;
    }

    const bool selected = itemIndex == scroll_.selectedIndex();
    snprintf(
        out.lines[line],
        kOledMenuLineChars,
        "%c %s",
        selected ? '>' : ' ',
        itemLabels[itemIndex]);
  }
}

} // namespace soarm
