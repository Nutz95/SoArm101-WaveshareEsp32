#pragma once

#include <cstdint>

namespace soarm {

class PeerPairingStore {
public:
  PeerPairingStore(const char *nameSpace, const char *key);

  bool load(uint8_t outMac[6]) const;
  bool save(const uint8_t mac[6]) const;
  void clear() const;

private:
  const char *nameSpace_;
  const char *key_;
};

} // namespace soarm
