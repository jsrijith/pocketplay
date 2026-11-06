#pragma once

#include <cstdint>

#include "cards.h"

// Drawing a playing card, ported from CrossPlay's cards/CardArt.cpp onto
// core/graphics. The geometry is CrossPlay's.
namespace cardart {

constexpr int CARD_WIDTH = 92;
constexpr int CARD_HEIGHT = 122;
constexpr int RADIUS = 8;
constexpr int EDGE = 2;

// How a card is covered by the one in front of it, which decides where its index
// goes. A fanned card only shows a strip, and the strip is a different shape each way.
enum class Fan : uint8_t {
  None,      // the whole card shows
  Down,      // covered from below: a short wide strip across the top
  Sideways,  // covered from the right: a tall narrow sliver down the left
};

// The suit mark, centred in the box. A box 32 or wider gets the 46 px art, and a
// smaller one the 18 px art: nothing is resampled. Red suits are outlines.
void drawSuit(uint8_t* frame, int left, int top, int width, int height, cards::Suit suit, bool outline);

// A face-up card. visible is how much of its height the next card leaves showing:
// the centre pip is drawn only on a card that shows whole.
void drawCardFace(uint8_t* frame, int left, int top, int width, int height, uint8_t card, int visible, Fan fan);

// A face-down card: a frame and a grey lattice, and the suit mark only on a whole one.
void drawCardBack(uint8_t* frame, int left, int top, int width, int height, int visible, cards::Suit mark = cards::Suit::Spades);

// An empty place: a dashed outline, so it can't be read as a blank card.
void drawCardSlot(uint8_t* frame, int left, int top, int width, int height);

// A rounded rectangle filled with CrossPlay's dark grey: every other pixel, as a
// checkerboard. Light grey (one dot in four) is ui's; this one is only a card back's.
void fillDarkGrey(uint8_t* frame, int left, int top, int width, int height, int radius);

}  // namespace cardart
