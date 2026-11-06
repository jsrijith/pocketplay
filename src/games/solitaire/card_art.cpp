#include "card_art.h"

#include "card_suits.h"
#include "core/graphics.h"
#include "core/text.h"
#include "ui/ui.h"

namespace cardart {

namespace {

namespace c = cards;

const SuitIcon& suitArt(c::Suit suit, int width, bool outline) {
  const bool big = width >= 32;
  switch (suit) {
    case c::Suit::Spades:
      if (big) return outline ? icon_spadeOutline_46 : icon_spadeSolid_46;
      return outline ? icon_spadeOutline_18 : icon_spadeSolid_18;
    case c::Suit::Hearts:
      if (big) return outline ? icon_heartOutline_46 : icon_heartSolid_46;
      return outline ? icon_heartOutline_18 : icon_heartSolid_18;
    case c::Suit::Diamonds:
      if (big) return outline ? icon_diamondOutline_46 : icon_diamondSolid_46;
      return outline ? icon_diamondOutline_18 : icon_diamondSolid_18;
    case c::Suit::Clubs:
    default:
      if (big) return outline ? icon_clubOutline_46 : icon_clubSolid_46;
      return outline ? icon_clubOutline_18 : icon_clubSolid_18;
  }
}

void drawPip(uint8_t* frame, int left, int top, int width, int height, uint8_t card) {
  drawSuit(frame, left, top, width, height, c::suitOf(card), c::isRed(card));
}

// Inside a rounded rectangle, the same test graphics.cpp's shapes use.
bool insideRounded(int x, int y, int left, int top, int width, int height, int radius) {
  if (x < left || x >= left + width || y < top || y >= top + height) return false;
  int dx = 0;
  int dy = 0;
  if (x < left + radius) dx = left + radius - x;
  if (x >= left + width - radius) dx = x - (left + width - radius - 1);
  if (y < top + radius) dy = top + radius - y;
  if (y >= top + height - radius) dy = y - (top + height - radius - 1);
  if (dx == 0 || dy == 0) return true;
  return dx * dx + dy * dy <= radius * radius;
}

}  // namespace

void drawSuit(uint8_t* frame, int left, int top, int width, int height, c::Suit suit, bool outline) {
  const SuitIcon& art = suitArt(suit, width, outline);
  drawIcon(frame, art.bits, art.w, art.h, left + (width - art.w) / 2, top + (height - art.h) / 2);
}

void fillDarkGrey(uint8_t* frame, int left, int top, int width, int height, int radius) {
  for (int y = top; y < top + height; y++) {
    for (int x = left; x < left + width; x++) {
      if (insideRounded(x, y, left, top, width, height, radius)) setPixel(frame, x, y, (x + y) % 2 == 0);
    }
  }
}

// The index is one line, rank then pip, so a card covered from below still shows
// its suit. A card covered from the right shows a sliver instead, so there the pip
// goes under the rank. Both sit RADIUS in from the edges they touch.
void drawCardFace(uint8_t* frame, int left, int top, int width, int height, uint8_t card, int visible, Fan fan) {
  constexpr int INDEX_INSET = RADIUS;
  fillRoundRect(frame, left, top, width, height, RADIUS, false);
  drawRoundRectOutline(frame, left, top, width, height, RADIUS, EDGE);

  ui::drawTextLeft(frame, Font::Size20, left + 9, top + INDEX_INSET, 28, c::rankLabel(c::rankOf(card)));

  if (fan == Fan::Sideways) {
    drawPip(frame, left + 10, top + INDEX_INSET + 40, 16, 18, card);
  } else {
    constexpr int PIP = 18;
    drawPip(frame, left + width - INDEX_INSET - PIP, top + INDEX_INSET, PIP, PIP, card);
  }

  // The big pip, placed from the bottom edge, and only on a card that shows whole.
  if (visible < height) return;
  constexpr int PIP_HEIGHT = 48;
  constexpr int PIP_BOTTOM_INSET = 10;
  if (width < 72 || height < PIP_HEIGHT + PIP_BOTTOM_INSET + 32) return;
  drawPip(frame, left + width / 2 - 20, top + height - PIP_BOTTOM_INSET - PIP_HEIGHT, 46, PIP_HEIGHT, card);
}

void drawCardBack(uint8_t* frame, int left, int top, int width, int height, int visible, c::Suit mark) {
  fillRoundRect(frame, left, top, width, height, RADIUS, false);
  drawRoundRectOutline(frame, left, top, width, height, RADIUS, EDGE);

  fillDarkGrey(frame, left + 7, top + 7, width - 14, height - 14, RADIUS / 2);
  drawRoundRectOutline(frame, left + 7, top + 7, width - 14, height - 14, RADIUS / 2, EDGE);
  if (visible < height) return;

  // Two sizes of mark, and a card too small for the big one gets the small one.
  const int size = (width >= 72 && height >= 96) ? 46 : 18;
  const int pad = size == 46 ? 5 : 4;
  const int haloLeft = left + (width - size) / 2 - pad;
  const int haloTop = top + (height - size) / 2 - pad;
  fillRoundRect(frame, haloLeft, haloTop, size + pad * 2, size + pad * 2, 6, false);
  drawSuit(frame, haloLeft + pad, haloTop + pad, size, size, mark, false);
}

void drawCardSlot(uint8_t* frame, int left, int top, int width, int height) {
  constexpr int DASH = 10;
  const int right = left + width;
  const int bottom = top + height;
  for (int x = left + RADIUS; x < right - RADIUS; x += DASH * 2) {
    const int run = (x + DASH > right - RADIUS) ? right - RADIUS - x : DASH;
    fillRect(frame, x, top, run, EDGE, true);
    fillRect(frame, x, bottom - EDGE, run, EDGE, true);
  }
  for (int y = top + RADIUS; y < bottom - RADIUS; y += DASH * 2) {
    const int run = (y + DASH > bottom - RADIUS) ? bottom - RADIUS - y : DASH;
    fillRect(frame, left, y, EDGE, run, true);
    fillRect(frame, right - EDGE, y, EDGE, run, true);
  }
}

}  // namespace cardart
