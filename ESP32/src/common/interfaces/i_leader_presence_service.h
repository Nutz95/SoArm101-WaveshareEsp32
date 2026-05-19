#pragma once

namespace soarm {

class ILeaderPresenceService {
public:
  virtual ~ILeaderPresenceService() = default;

  virtual bool begin() = 0;
  virtual void tick() = 0;
  virtual bool isFollowerLinked() const = 0;
  virtual bool hasValidFollowerIp() const = 0;
  virtual const char *followerIp() const = 0;
  virtual bool isPaired() const = 0;
  virtual const char *pairedPeerMac() const = 0;
  virtual const char *localMac() const = 0;
  virtual bool resetPairing() = 0;
};

} // namespace soarm
