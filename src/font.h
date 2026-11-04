#pragma once

#include <cstdint>

// A 5x7 bitmap font holding only the letters in "HELLO WORLD".
// Each glyph is 7 rows; the low 5 bits of a row are its pixels, left to right.
namespace font {

constexpr int GLYPH_WIDTH = 5;
constexpr int GLYPH_HEIGHT = 7;

struct Glyph {
  char letter;
  uint8_t rows[GLYPH_HEIGHT];
};

constexpr Glyph GLYPHS[] = {
    {'H', {0b10001, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001}},
    {'E', {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111}},
    {'L', {0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b11111}},
    {'O', {0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110}},
    {'W', {0b10001, 0b10001, 0b10001, 0b10101, 0b10101, 0b11011, 0b10001}},
    {'R', {0b11110, 0b10001, 0b10001, 0b11110, 0b10100, 0b10010, 0b10001}},
    {'D', {0b11110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b11110}},
};

// Returns nullptr for characters not in the table, such as a space.
constexpr const Glyph* find(char letter) {
  for (const Glyph& glyph : GLYPHS) {
    if (glyph.letter == letter) return &glyph;
  }
  return nullptr;
}

}  // namespace font
