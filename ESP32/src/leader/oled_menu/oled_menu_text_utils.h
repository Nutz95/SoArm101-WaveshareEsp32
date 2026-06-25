#pragma once

#include <cstddef>

namespace soarm {

// Copy a C string into a fixed-size menu line buffer (always null-terminated).
void oledMenuCopyLine(char *dest, size_t destSize, const char *source);
// Return the IP string or "?" when missing/empty.
const char *oledMenuIpOrUnknown(const char *ip);

} // namespace soarm
