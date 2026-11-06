#pragma once
// The panel's size, for core/graphics.h. Nothing is ever sent to a panel.
#include <cstdint>

struct EInkDisplay {
  static constexpr int DISPLAY_WIDTH = 800;
  static constexpr int DISPLAY_HEIGHT = 480;
  static constexpr int DISPLAY_WIDTH_BYTES = DISPLAY_WIDTH / 8;
};
