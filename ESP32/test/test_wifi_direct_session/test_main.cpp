#include <unity.h>

#include <cstring>

#include "../../src/common/wifi/wifi_direct_offer_packet.h"
#include "../../src/common/wifi/wifi_direct_session.h"

using namespace soarm;

void test_generate_credentials_unique_psk() {
  WifiDirectCredentials first{};
  WifiDirectCredentials second{};
  TEST_ASSERT_TRUE(generateWifiDirectCredentials(100U, first));
  TEST_ASSERT_TRUE(generateWifiDirectCredentials(101U, second));
  TEST_ASSERT_NOT_EQUAL(0, strcmp(first.psk, second.psk));
  TEST_ASSERT_NOT_EQUAL(0, strcmp(first.ssid, second.ssid));
}

void test_offer_ack_round_trip() {
  WifiDirectCredentials credentials{};
  TEST_ASSERT_TRUE(generateWifiDirectCredentials(42U, credentials));

  WifiDirectOfferPacket offer{};
  buildWifiDirectOfferPacket(credentials, offer);

  WifiDirectCredentials parsed{};
  TEST_ASSERT_TRUE(validateWifiDirectOfferPacket(offer, parsed));
  TEST_ASSERT_EQUAL_UINT32(credentials.sessionId, parsed.sessionId);
  TEST_ASSERT_EQUAL_UINT16(kWifiDirectTeleopPort, offer.teleopPort);

  WifiDirectAckPacket ack{};
  buildWifiDirectAckPacket(credentials.sessionId, WifiDirectAckStatus::Connected, "192.168.4.2", ack);
  TEST_ASSERT_TRUE(validateWifiDirectAckPacket(ack, credentials.sessionId));
}

void test_validate_offer_rejects_bad_magic() {
  WifiDirectCredentials credentials{};
  TEST_ASSERT_TRUE(generateWifiDirectCredentials(7U, credentials));
  WifiDirectOfferPacket offer{};
  buildWifiDirectOfferPacket(credentials, offer);
  offer.magic = 0U;

  WifiDirectCredentials parsed{};
  TEST_ASSERT_FALSE(validateWifiDirectOfferPacket(offer, parsed));
}

void test_validate_ack_rejects_session_mismatch() {
  WifiDirectAckPacket ack{};
  buildWifiDirectAckPacket(99U, WifiDirectAckStatus::Connected, "192.168.4.3", ack);
  TEST_ASSERT_FALSE(validateWifiDirectAckPacket(ack, 100U));
}

void test_session_end_ack_round_trip() {
  WifiDirectAckPacket ack{};
  buildWifiDirectAckPacket(55U, WifiDirectAckStatus::SessionEnd, nullptr, ack);
  TEST_ASSERT_TRUE(validateWifiDirectAckPacket(ack, 55U));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WifiDirectAckStatus::SessionEnd), ack.status);
}

void setUp() {
}

void tearDown() {
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_generate_credentials_unique_psk);
  RUN_TEST(test_offer_ack_round_trip);
  RUN_TEST(test_validate_offer_rejects_bad_magic);
  RUN_TEST(test_validate_ack_rejects_session_mismatch);
  RUN_TEST(test_session_end_ack_round_trip);
  return UNITY_END();
}
