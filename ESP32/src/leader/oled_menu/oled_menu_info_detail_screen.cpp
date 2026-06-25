#include "oled_menu_info_detail_screen.h"

#include "oled_menu_text_utils.h"

#include <cstdio>

namespace soarm {

void OledMenuInfoDetailScreen::render(const OledMenuContext &context, OledMenuRenderOutput &out) const {
  clearOutput(out);

  char line1[kOledMenuLineChars]{};
  char line2[kOledMenuLineChars]{};
  char line3[kOledMenuLineChars]{};
  char line4[kOledMenuLineChars]{};

  snprintf(line1, sizeof(line1), "L:%s", oledMenuIpOrUnknown(context.leaderIp));
  snprintf(line2, sizeof(line2), "F:%s", oledMenuIpOrUnknown(context.followerIpHint));
  snprintf(line3, sizeof(line3), "XBL:%s", context.xboxBlePaired ? "paired" : "off");
  snprintf(
      line4,
      sizeof(line4),
      "ENOW:%s :9090:%s",
      context.espNowLinked ? "link" : "wait",
      context.telemetryListening ? "on" : "off");

  oledMenuCopyLine(out.lines[0], kOledMenuLineChars, line1);
  oledMenuCopyLine(out.lines[1], kOledMenuLineChars, line2);
  oledMenuCopyLine(out.lines[2], kOledMenuLineChars, line3);
  oledMenuCopyLine(out.lines[3], kOledMenuLineChars, line4);
}

} // namespace soarm
