#pragma once

#include <cstdint>

// The shape of the SDK's freeink::Icon, which card_suits.h was generated as. It's
// named here so the suit art needs no SDK library, and card_suits.h is unchanged
// apart from this name and its include.
//
// Rows top to bottom, each (w + 7) / 8 bytes, most significant bit first, and 0 is
// ink: the format drawIcon() in core/graphics.h reads.
struct SuitIcon {
  uint16_t w;
  uint16_t h;
  int16_t opticalCenterY;  // unused here: suits are centred in their box
  const uint8_t* bits;
};
