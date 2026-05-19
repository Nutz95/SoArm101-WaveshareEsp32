#pragma once

namespace soarm {

class IFollowerPresenceService {
public:
  virtual ~IFollowerPresenceService() = default;

  virtual bool begin() = 0;
  virtual void tick(const char *localIp) = 0;
  virtual bool isPaired() const = 0;
  virtual const char *pairedPeerMac() const = 0;
  virtual const char *localMac() const = 0;
  virtual bool resetPairing() = 0;
};

} // namespace soarm
