#pragma once

#include <cstdint>

namespace soarm {

// Scroll window for a vertical list with a single cursor (4 visible lines).
class OledListScrollModel {
public:
  // Configure item count and initial selection; clamps out-of-range indices.
  void reset(uint8_t itemCount, uint8_t selectedIndex = 0U);

  uint8_t itemCount() const { return itemCount_; }
  uint8_t selectedIndex() const { return selectedIndex_; }
  // First item index shown on line 0 (scroll offset).
  uint8_t firstVisibleIndex() const { return firstVisibleIndex_; }

  // Move selection up, wrapping to the last item.
  void moveUp();
  // Move selection down, wrapping to the first item.
  void moveDown();

  // True when itemIndex is within the current four-line window.
  bool isItemVisible(uint8_t itemIndex) const;
  // Item index for visible line 0..3, or itemCount() when the line is empty.
  uint8_t visibleItemIndex(uint8_t visibleLine) const;

private:
  void clampSelection();
  void updateScrollWindow();

  uint8_t itemCount_{0U};
  uint8_t selectedIndex_{0U};
  uint8_t firstVisibleIndex_{0U};
};

} // namespace soarm
