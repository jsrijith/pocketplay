#include "ui.h"

#include "core/graphics.h"
#include "core/text.h"

namespace ui {

namespace {
constexpr int BORDER = 3;
}

bool isInBox(int x, int y, int left, int top, int width, int height) { return x >= left && x < left + width && y >= top && y < top + height; }

void drawButton(uint8_t* frame, int left, int top, int width, int height, const char* label) {
  drawRectOutline(frame, left, top, width, height, BORDER);
  drawTextInBox(frame, Font::Size20, left, top, width, height, label);
}

void drawHeader(uint8_t* frame, const char* title, bool hasRightButton) {
  drawButton(frame, MARGIN, HEADER_TOP, HEADER_BUTTON_WIDTH, HEADER_BUTTON_HEIGHT, "BACK");

  const int side = MARGIN + HEADER_BUTTON_WIDTH;
  const int left = side + GAP;
  const int right = hasRightButton ? SCREEN_WIDTH - side - GAP : SCREEN_WIDTH - MARGIN;
  drawTextInBox(frame, Font::Size20, left, HEADER_TOP, right - left, HEADER_BUTTON_HEIGHT, title);
}

bool isOnBack(int x, int y) { return isInBox(x, y, MARGIN, HEADER_TOP, HEADER_BUTTON_WIDTH, HEADER_BUTTON_HEIGHT); }

void drawWarning(uint8_t* frame, const char* text) { drawTextInBox(frame, Font::Size14, 0, WARNING_TOP, SCREEN_WIDTH, lineHeight(Font::Size14), text); }

}  // namespace ui
