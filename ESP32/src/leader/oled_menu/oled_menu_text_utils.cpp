#include "oled_menu_text_utils.h"

#include <cstring>

namespace soarm {

void oledMenuCopyLine(char *dest, size_t destSize, const char *source) {
  if (dest == nullptr || destSize == 0U) {
    return;
  }
  if (source == nullptr) {
    dest[0] = '\0';
    return;
  }
  strncpy(dest, source, destSize - 1U);
  dest[destSize - 1U] = '\0';
}

const char *oledMenuIpOrUnknown(const char *ip) {
  return (ip != nullptr && ip[0] != '\0') ? ip : "?";
}

} // namespace soarm
