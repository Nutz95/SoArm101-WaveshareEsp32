#include "oled_list_scroll_model.h"

namespace soarm {

namespace {

constexpr uint8_t kVisibleLineCount = 4U;

} // namespace

void OledListScrollModel::reset(uint8_t itemCount, uint8_t selectedIndex) {
  itemCount_ = itemCount;
  selectedIndex_ = selectedIndex;
  clampSelection();
  updateScrollWindow();
}

void OledListScrollModel::moveUp() {
  if (itemCount_ == 0U) {
    return;
  }

  if (selectedIndex_ == 0U) {
    selectedIndex_ = static_cast<uint8_t>(itemCount_ - 1U);
  } else {
    selectedIndex_ = static_cast<uint8_t>(selectedIndex_ - 1U);
  }
  updateScrollWindow();
}

void OledListScrollModel::moveDown() {
  if (itemCount_ == 0U) {
    return;
  }

  selectedIndex_ = static_cast<uint8_t>((selectedIndex_ + 1U) % itemCount_);
  updateScrollWindow();
}

bool OledListScrollModel::isItemVisible(uint8_t itemIndex) const {
  if (itemCount_ == 0U || itemIndex >= itemCount_) {
    return false;
  }
  return itemIndex >= firstVisibleIndex_ &&
         itemIndex < static_cast<uint8_t>(firstVisibleIndex_ + kVisibleLineCount);
}

uint8_t OledListScrollModel::visibleItemIndex(uint8_t visibleLine) const {
  if (visibleLine >= kVisibleLineCount) {
    return itemCount_;
  }
  const uint8_t itemIndex = static_cast<uint8_t>(firstVisibleIndex_ + visibleLine);
  if (itemIndex >= itemCount_) {
    return itemCount_;
  }
  return itemIndex;
}

void OledListScrollModel::clampSelection() {
  if (itemCount_ == 0U) {
    selectedIndex_ = 0U;
    return;
  }
  if (selectedIndex_ >= itemCount_) {
    selectedIndex_ = static_cast<uint8_t>(itemCount_ - 1U);
  }
}

void OledListScrollModel::updateScrollWindow() {
  if (itemCount_ == 0U) {
    firstVisibleIndex_ = 0U;
    return;
  }

  if (itemCount_ <= kVisibleLineCount) {
    firstVisibleIndex_ = 0U;
    return;
  }

  if (selectedIndex_ < firstVisibleIndex_) {
    firstVisibleIndex_ = selectedIndex_;
  } else if (selectedIndex_ >= static_cast<uint8_t>(firstVisibleIndex_ + kVisibleLineCount)) {
    firstVisibleIndex_ = static_cast<uint8_t>(selectedIndex_ - (kVisibleLineCount - 1U));
  }
}

} // namespace soarm
