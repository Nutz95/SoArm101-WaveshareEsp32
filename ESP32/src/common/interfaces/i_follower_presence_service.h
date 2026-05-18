#pragma once

namespace soarm {

class IFollowerPresenceService {
public:
  virtual ~IFollowerPresenceService() = default;

  virtual bool begin() = 0;
  virtual void tick(const char *localIp) = 0;
};

} // namespace soarm
