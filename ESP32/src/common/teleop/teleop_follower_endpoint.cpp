#include "teleop_follower_endpoint.h"

#include <WiFi.h>
#include <cstring>

namespace soarm {

namespace {

constexpr const char *kFollowerMdnsHost = "soarm-follower.local";

bool isUsableIp(const char *ip) {
  return ip != nullptr && ip[0] != '\0' && strcmp(ip, "0.0.0.0") != 0;
}

} // namespace

bool resolveFollowerEndpoint(const char *presenceIp, char *out, size_t outLen) {
  if (out == nullptr || outLen == 0U) {
    return false;
  }

  out[0] = '\0';

  if (isUsableIp(presenceIp)) {
    strncpy(out, presenceIp, outLen - 1U);
    out[outLen - 1U] = '\0';
    return true;
  }

  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  IPAddress resolved;
  if (!WiFi.hostByName(kFollowerMdnsHost, resolved)) {
    return false;
  }

  const String ipText = resolved.toString();
  strncpy(out, ipText.c_str(), outLen - 1U);
  out[outLen - 1U] = '\0';
  return out[0] != '\0';
}

} // namespace soarm
