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

namespace {

constexpr int ROW_WIDTH = SCREEN_WIDTH - 2 * MARGIN;
constexpr int BAND_RULE_GAP = 3;
constexpr int BACK_TOP = 10;
constexpr int BACK_HEIGHT = 46;
constexpr int ROW_TEXT_INSET = 14;

// The cuts a title tries, largest first.
constexpr Font TITLE_CUTS[] = {Font::Size30, Font::Size20, Font::Size14};

Font largestFitting(const char* text, int width) {
  for (const Font cut : TITLE_CUTS) {
    if (textWidth(cut, text) <= width) return cut;
  }
  return Font::Size10;
}

}  // namespace

void drawTextLeft(uint8_t* frame, Font font, int left, int top, int height, const char* text, bool black) {
  const int cap = capHeight(font);
  drawText(frame, font, left, top + (height - cap) / 2 + cap, text, Align::Left, black);
}

void drawBand(uint8_t* frame, const char* title, const char* rightLabel, bool hasBack, Font labelFont) {
  fillRect(frame, 0, 0, SCREEN_WIDTH, BAND_HEIGHT, true);
  fillRect(frame, 0, BAND_HEIGHT + BAND_RULE_GAP, SCREEN_WIDTH, RULE, true);

  int left = MARGIN + 4;
  if (hasBack) {
    drawRoundRectOutline(frame, MARGIN, BACK_TOP, HEADER_BUTTON_WIDTH, BACK_HEIGHT, PILL_RADIUS, RULE, false);
    drawTextInBox(frame, Font::Size20, MARGIN, BACK_TOP, HEADER_BUTTON_WIDTH, BACK_HEIGHT, "BACK", false);
    left = MARGIN + HEADER_BUTTON_WIDTH + GAP;
  }

  int right = SCREEN_WIDTH - MARGIN - 4;
  if (rightLabel != nullptr && rightLabel[0] != '\0') {
    const int baseline = (BAND_HEIGHT + capHeight(labelFont)) / 2;
    drawText(frame, labelFont, right, baseline, rightLabel, Align::Right, false);
    right -= textWidth(labelFont, rightLabel) + GAP;
  }

  drawTextLeft(frame, largestFitting(title, right - left), left, 0, BAND_HEIGHT, title, false);
}

// Labels are Size14, the cut CrossPlay's buttons use, stepping down only if one
// doesn't fit.
void drawPill(uint8_t* frame, int left, int top, int width, const char* label, bool filled, int height) {
  if (filled) {
    fillRoundRect(frame, left, top, width, height, PILL_RADIUS, true);
  } else {
    drawRoundRectOutline(frame, left, top, width, height, PILL_RADIUS, RULE);
  }
  const Font font = textWidth(Font::Size14, label) <= width - 2 * PILL_RADIUS ? Font::Size14 : Font::Size10;
  drawTextInBox(frame, font, left, top, width, height, label, !filled);
}

void drawRow(uint8_t* frame, int top, const char* label, const uint8_t* icon) {
  drawRectOutline(frame, MARGIN, top, ROW_WIDTH, ROW_HEIGHT, HAIRLINE);
  drawTextLeft(frame, Font::Size20, MARGIN + ROW_TEXT_INSET, top, ROW_HEIGHT, label);
  if (icon != nullptr) {
    drawIcon(frame, icon, ICON_SIZE, ICON_SIZE, MARGIN + ROW_WIDTH - ROW_TEXT_INSET - ICON_SIZE, top + (ROW_HEIGHT - ICON_SIZE) / 2);
  }
}

}  // namespace ui
