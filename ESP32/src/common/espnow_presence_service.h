#pragma once

#include <cstdint>

namespace soarm {

class EspNowPresenceService {
public:
  enum class Role : uint8_t {
    Leader = 0,
    Follower = 1
  };

  using FollowerIpCallback = void (*)(const char *ip);

  bool begin(Role role, FollowerIpCallback onFollowerIp = nullptr);
  void tick(const char *localIp);

  bool isFollowerLinked() const;
  bool hasValidFollowerIp() const;
  const char *followerIp() const;

private:
  static void onDataRecvStatic(const uint8_t *mac, const uint8_t *data, int len);
  void handleReceived(const uint8_t *data, int len);

  bool addBroadcastPeer();
  void sendFollowerPresence(const char *localIp);

  Role role_{Role::Leader};
  FollowerIpCallback onFollowerIp_{nullptr};
  bool started_{false};
  uint32_t lastTxMs_{0};
  uint32_t lastFollowerSeenMs_{0};
  char followerIp_[16]{};
};

} // namespace soarm
