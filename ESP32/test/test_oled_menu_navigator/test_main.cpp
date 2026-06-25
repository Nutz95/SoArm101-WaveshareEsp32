#include <unity.h>

#include "leader/oled_menu/ioled_menu_profile_actions.h"
#include "leader/oled_menu/oled_list_scroll_model.h"
#include "leader/oled_menu/oled_menu_context.h"
#include "leader/oled_menu/oled_menu_input.h"
#include "leader/oled_menu/oled_menu_navigator.h"
#include "leader/oled_menu/oled_menu_root_items.h"
#include "leader/oled_menu/oled_menu_screen_id.h"

using soarm::OledListScrollModel;
using soarm::OledMenuContext;
using soarm::OledMenuInputEvent;
using soarm::OledMenuNavigator;
using soarm::OledMenuRenderOutput;
using soarm::OledMenuScreenId;
using soarm::IOledMenuProfileActions;
using soarm::OledMenuProfileSelection;
using soarm::OledMenuRootItem;

namespace {

class RecordingProfileActions : public IOledMenuProfileActions {
public:
  OledMenuProfileSelection lastSelection{OledMenuProfileSelection::TeleopEspNow};
  uint8_t callCount{0U};

  bool activateMenuProfile(OledMenuProfileSelection selection) override {
    lastSelection = selection;
    callCount = static_cast<uint8_t>(callCount + 1U);
    return true;
  }
};

} // namespace

void test_navigator_teleop_submenu_activates_profile() {
  OledMenuNavigator navigator;
  RecordingProfileActions actions;
  navigator.setProfileActionsSink(&actions);
  navigator.reset();

  TEST_ASSERT_TRUE(navigator.onInput(OledMenuInputEvent::NavigateDown));
  TEST_ASSERT_TRUE(navigator.onInput(OledMenuInputEvent::Select));
  TEST_ASSERT_EQUAL(static_cast<int>(OledMenuScreenId::TeleopList),
                    static_cast<int>(navigator.currentScreen()));

  TEST_ASSERT_TRUE(navigator.onInput(OledMenuInputEvent::Select));
  TEST_ASSERT_EQUAL_UINT8(1U, actions.callCount);
  TEST_ASSERT_EQUAL(static_cast<int>(OledMenuProfileSelection::TeleopEspNow),
                    static_cast<int>(actions.lastSelection));
}

void test_navigator_passthrough_leaf_activates_profile() {
  OledMenuNavigator navigator;
  RecordingProfileActions actions;
  navigator.setProfileActionsSink(&actions);
  navigator.reset();

  TEST_ASSERT_TRUE(navigator.onInput(OledMenuInputEvent::NavigateDown));
  TEST_ASSERT_TRUE(navigator.onInput(OledMenuInputEvent::NavigateDown));
  TEST_ASSERT_TRUE(navigator.onInput(OledMenuInputEvent::Select));
  TEST_ASSERT_EQUAL_UINT8(1U, actions.callCount);
  TEST_ASSERT_EQUAL(static_cast<int>(OledMenuProfileSelection::Passthrough),
                    static_cast<int>(actions.lastSelection));
}

void test_navigator_ik_teleop_shows_stub_screen() {
  OledMenuNavigator navigator;
  navigator.reset();

  TEST_ASSERT_TRUE(navigator.onInput(OledMenuInputEvent::NavigateDown));
  TEST_ASSERT_TRUE(navigator.onInput(OledMenuInputEvent::Select));

  for (uint8_t i = 0U; i < 3U; ++i) {
    navigator.onInput(OledMenuInputEvent::NavigateDown);
  }
  TEST_ASSERT_TRUE(navigator.onInput(OledMenuInputEvent::Select));
  TEST_ASSERT_EQUAL(static_cast<int>(OledMenuScreenId::IkNotImplementedDetail),
                    static_cast<int>(navigator.currentScreen()));

  OledMenuContext context{};
  OledMenuRenderOutput output{};
  navigator.render(context, output);
  TEST_ASSERT_EQUAL_STRING("IK Teleop", output.lines[0]);
  TEST_ASSERT_EQUAL_STRING("not implemented", output.lines[1]);
}

void test_scroll_window_keeps_cursor_visible_at_bottom() {
  OledListScrollModel model;
  model.reset(6U, 5U);
  TEST_ASSERT_EQUAL_UINT8(2U, model.firstVisibleIndex());
  TEST_ASSERT_TRUE(model.isItemVisible(5U));
  TEST_ASSERT_EQUAL_UINT8(5U, model.visibleItemIndex(3U));
}

void test_scroll_wraps_down_from_last_item() {
  OledListScrollModel model;
  model.reset(6U, 5U);
  model.moveDown();
  TEST_ASSERT_EQUAL_UINT8(0U, model.selectedIndex());
  TEST_ASSERT_EQUAL_UINT8(0U, model.firstVisibleIndex());
}

void test_scroll_wraps_up_from_first_item() {
  OledListScrollModel model;
  model.reset(6U, 0U);
  model.moveUp();
  TEST_ASSERT_EQUAL_UINT8(5U, model.selectedIndex());
  TEST_ASSERT_EQUAL_UINT8(2U, model.firstVisibleIndex());
}

void test_navigator_root_mode_down_moves_highlight() {
  OledMenuNavigator navigator;
  navigator.reset();
  TEST_ASSERT_TRUE(navigator.onInput(OledMenuInputEvent::ModeDown));
  OledMenuContext context{};
  OledMenuRenderOutput output{};
  navigator.render(context, output);
  TEST_ASSERT_EQUAL_STRING("  Info", output.lines[0]);
  TEST_ASSERT_EQUAL_STRING("> Teleop", output.lines[1]);
}

void test_navigator_info_push_and_back() {
  OledMenuNavigator navigator;
  navigator.reset();
  TEST_ASSERT_TRUE(navigator.onInput(OledMenuInputEvent::Select));
  TEST_ASSERT_EQUAL(static_cast<int>(OledMenuScreenId::InfoDetail), static_cast<int>(navigator.currentScreen()));

  OledMenuContext context{};
  context.leaderIp = "192.168.1.10";
  context.followerIpHint = "192.168.1.20";
  context.xboxBlePaired = true;
  context.espNowLinked = true;
  context.telemetryListening = true;

  OledMenuRenderOutput output{};
  navigator.render(context, output);
  TEST_ASSERT_EQUAL_STRING("L:192.168.1.10", output.lines[0]);

  TEST_ASSERT_TRUE(navigator.onInput(OledMenuInputEvent::Back));
  TEST_ASSERT_TRUE(navigator.isAtRoot());
}

void test_navigator_ota_leaf_activates_profile() {
  OledMenuNavigator navigator;
  RecordingProfileActions actions;
  navigator.setProfileActionsSink(&actions);
  navigator.reset();

  for (uint8_t i = 0U; i < static_cast<uint8_t>(OledMenuRootItem::Ota); ++i) {
    navigator.onInput(OledMenuInputEvent::NavigateDown);
  }
  TEST_ASSERT_TRUE(navigator.onInput(OledMenuInputEvent::Select));
  TEST_ASSERT_EQUAL_UINT8(1U, actions.callCount);
  TEST_ASSERT_EQUAL(static_cast<int>(OledMenuProfileSelection::OtaReady),
                    static_cast<int>(actions.lastSelection));
}

void test_navigator_resume_at_teleop_list() {
  OledMenuNavigator navigator;
  navigator.reset();
  navigator.resumeAt(OledMenuScreenId::TeleopList);
  TEST_ASSERT_EQUAL(static_cast<int>(OledMenuScreenId::TeleopList),
                    static_cast<int>(navigator.currentScreen()));
  TEST_ASSERT_FALSE(navigator.isAtRoot());
}

void test_navigator_pairing_status_screen() {
  OledMenuNavigator navigator;
  navigator.reset();

  for (uint8_t i = 0U; i < 4U; ++i) {
    navigator.onInput(OledMenuInputEvent::NavigateDown);
  }
  TEST_ASSERT_TRUE(navigator.onInput(OledMenuInputEvent::Select));
  TEST_ASSERT_TRUE(navigator.onInput(OledMenuInputEvent::Select));

  OledMenuContext context{};
  context.espNowPaired = true;
  context.pairedPeerMac = "AA:BB:CC:DD:EE:FF";
  context.espNowLinked = true;
  context.followerIpHint = "192.168.1.51";

  OledMenuRenderOutput output{};
  navigator.render(context, output);
  TEST_ASSERT_EQUAL_STRING("Pair:paired", output.lines[0]);
  TEST_ASSERT_EQUAL_STRING("MAC:AA:BB:CC:DD:EE:FF", output.lines[1]);
}

void setUp() {
}

void tearDown() {
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_scroll_window_keeps_cursor_visible_at_bottom);
  RUN_TEST(test_scroll_wraps_down_from_last_item);
  RUN_TEST(test_scroll_wraps_up_from_first_item);
  RUN_TEST(test_navigator_root_mode_down_moves_highlight);
  RUN_TEST(test_navigator_teleop_submenu_activates_profile);
  RUN_TEST(test_navigator_passthrough_leaf_activates_profile);
  RUN_TEST(test_navigator_ik_teleop_shows_stub_screen);
  RUN_TEST(test_navigator_ota_leaf_activates_profile);
  RUN_TEST(test_navigator_resume_at_teleop_list);
  RUN_TEST(test_navigator_info_push_and_back);
  RUN_TEST(test_navigator_pairing_status_screen);
  return UNITY_END();
}
