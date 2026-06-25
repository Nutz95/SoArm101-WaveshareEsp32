#include "oled_menu_ik_not_implemented_screen.h"

#include "oled_menu_text_utils.h"

namespace soarm {

void OledMenuIkNotImplementedScreen::render(const OledMenuContext &context,
                                            OledMenuRenderOutput &out) const {
  (void)context;
  clearOutput(out);
  oledMenuCopyLine(out.lines[0], kOledMenuLineChars, "IK Teleop");
  oledMenuCopyLine(out.lines[1], kOledMenuLineChars, "not implemented");
  oledMenuCopyLine(out.lines[2], kOledMenuLineChars, "(coming phase 5)");
  oledMenuCopyLine(out.lines[3], kOledMenuLineChars, "B: back");
}

} // namespace soarm
