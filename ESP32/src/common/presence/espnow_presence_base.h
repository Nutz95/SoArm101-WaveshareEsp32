#pragma once

#include <cstdint>

namespace soarm {

class EspNowPresenceBase {
public:
  virtual ~EspNowPresenceBase() = default;

protected:
  bool initEspNow();
  bool addPeer(const uint8_t mac[6]);

private:
  static void onDataRecvStatic(const uint8_t *mac, const uint8_t *data, int len);
  virtual void onPresenceFrame(const uint8_t *mac, const uint8_t *data, int len) = 0;

  static EspNowPresenceBase *activeInstance_;
};

} // namespace soarm
