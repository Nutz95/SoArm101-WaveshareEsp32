#include "oled_presenter.h"

#include <cstdio>

namespace soarm {

void OledPresenter::showFeedbackTeleop(const uint8_t loads[6], uint8_t feedbackHz, uint8_t mirrorLoopMs) {
  if (display_ == nullptr || loads == nullptr) {
    return;
  }

  char l1[24]{};
  char l2[24]{};
  char l3[24]{};
  char l4[24]{};

  snprintf(
      l1,
      sizeof(l1),
      "J1:%d J2:%d J3:%d",
      static_cast<int>(loads[0]),
      static_cast<int>(loads[1]),
      static_cast<int>(loads[2]));
  snprintf(
      l2,
      sizeof(l2),
      "J4:%d J5:%d J6:%d",
      static_cast<int>(loads[3]),
      static_cast<int>(loads[4]),
      static_cast<int>(loads[5]));
  if (feedbackHz == 0U) {
    snprintf(l3, sizeof(l3), "fb:--Hz lat:%ums", static_cast<unsigned>(mirrorLoopMs));
  } else {
    snprintf(
        l3,
        sizeof(l3),
        "fb:%uHz lat:%ums",
        static_cast<unsigned>(feedbackHz),
        static_cast<unsigned>(mirrorLoopMs));
  }
  snprintf(l4, sizeof(l4), "GRIP:%d", static_cast<int>(loads[5]));

  printLines(l1, l2, l3, l4);
}

} // namespace soarm
