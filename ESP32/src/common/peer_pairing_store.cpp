#include "peer_pairing_store.h"

#include <Preferences.h>

namespace soarm {

PeerPairingStore::PeerPairingStore(const char *nameSpace, const char *key)
    : nameSpace_(nameSpace), key_(key) {
}

bool PeerPairingStore::load(uint8_t outMac[6]) const {
  Preferences pref;
  if (!pref.begin(nameSpace_, true)) {
    return false;
  }

  const size_t len = pref.getBytesLength(key_);
  if (len != 6U) {
    pref.end();
    return false;
  }

  const size_t readLen = pref.getBytes(key_, outMac, 6U);
  pref.end();
  return readLen == 6U;
}

bool PeerPairingStore::save(const uint8_t mac[6]) const {
  Preferences pref;
  if (!pref.begin(nameSpace_, false)) {
    return false;
  }

  const size_t written = pref.putBytes(key_, mac, 6U);
  pref.end();
  return written == 6U;
}

void PeerPairingStore::clear() const {
  Preferences pref;
  if (!pref.begin(nameSpace_, false)) {
    return;
  }

  pref.remove(key_);
  pref.end();
}

} // namespace soarm
