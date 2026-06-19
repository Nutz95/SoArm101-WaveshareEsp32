#pragma once

#include <cstdio>
#include <cstddef>

namespace soarm {

inline void copyCString(char *dst, size_t dstSize, const char *src) {
  if (dst == nullptr || dstSize == 0U) {
    return;
  }
  snprintf(dst, dstSize, "%s", src != nullptr ? src : "");
}

} // namespace soarm
