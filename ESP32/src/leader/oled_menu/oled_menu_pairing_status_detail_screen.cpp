#include "oled_menu_pairing_status_detail_screen.h"

#include "oled_menu_text_utils.h"

#include <cstdio>

namespace soarm {

void OledMenuPairingStatusDetailScreen::render(const OledMenuContext &context,
                                               OledMenuRenderOutput &out) const {
  clearOutput(out);

  char line1[kOledMenuLineChars]{};
  char line2[kOledMenuLineChars]{};
  char line3[kOledMenuLineChars]{};
  char line4[kOledMenuLineChars]{};

  snprintf(
      line1,
      sizeof(line1),
      "Pair:%s",
      context.espNowPaired ? "paired" : "unpaired");
  snprintf(
      line2,
      sizeof(line2),
      "MAC:%s",
      oledMenuIpOrUnknown(context.pairedPeerMac));
  snprintf(line3, sizeof(line3), "Link:%s", context.espNowLinked ? "yes" : "no");
  snprintf(line4, sizeof(line4), "F:%s", oledMenuIpOrUnknown(context.followerIpHint));

  oledMenuCopyLine(out.lines[0], kOledMenuLineChars, line1);
  oledMenuCopyLine(out.lines[1], kOledMenuLineChars, line2);
  oledMenuCopyLine(out.lines[2], kOledMenuLineChars, line3);
  oledMenuCopyLine(out.lines[3], kOledMenuLineChars, line4);
}

} // namespace soarm
