#pragma once

#include <cstdint>

#include "connections_rules.h"
#include "core/text.h"

// How the board's words are cut into lines, kept apart from the drawing so a PC test
// can walk every board in the archive through it (tests/connections/).
//
// Ported from CrossPlay's ConnectionsScreens.cpp. Its rules:
//   - Never shorten a word. Every line is measured to fit before it's drawn, and there
//     is no ellipsis anywhere.
//   - One cut for all sixteen tiles, the largest at which every word on the board is
//     set whole. A long word set smaller than the fifteen beside it reads as mattering
//     more.
//   - Shrink before breaking. A phrase wraps at its own spaces, and a word is only cut
//     when no cut can set it whole, and then evenly ("FRANKEN / STEIN'S").
//
// CrossPlay chooses between two serif cuts. PocketPlay has only Jersey, which is wider,
// so it steps through three: Size20, Size14 and Size10.
namespace connectionsfit {

// The board's tiles, shared by the drawing and the test so both measure the same box.
// Tighter margins than the rest of the screen: a tile holds a word that can't be
// shortened, so every pixel of width buys characters. Square, like CrossPlay's.
constexpr int BOARD_WIDTH = 480;  // SCREEN_WIDTH, checked in connections_screens.cpp
constexpr int GRID_MARGIN = 12;
constexpr int TILE_GAP = 4;
constexpr int TILE_PAD = 3;  // the text's inset on each side; 2 at top and bottom
constexpr int GRID_WIDTH = BOARD_WIDTH - 2 * GRID_MARGIN;
constexpr int TILE_WIDTH = (GRID_WIDTH - 3 * TILE_GAP) / 4;
constexpr int ROW_HEIGHT = TILE_WIDTH;
constexpr int TILE_INNER_WIDTH = TILE_WIDTH - 2 * TILE_PAD;
constexpr int TILE_INNER_HEIGHT = ROW_HEIGHT - 4;
constexpr int SOLVED_INNER_WIDTH = GRID_WIDTH - 2 * TILE_PAD;

constexpr int CUT_COUNT = 3;
constexpr Font CUTS[CUT_COUNT] = {Font::Size20, Font::Size14, Font::Size10};

// Lines a tile prefers. A cap on the look, not on what fits: when a word has to be cut,
// the tile takes as many lines as it's tall enough for.
constexpr int TILE_LINES = 3;
constexpr int MAX_LINES = 6;

// The longest string set: a group's four members joined with ", ".
constexpr int MAX_LINE_CHARS = connections::kMaxWordLen * 4 + 8;
static_assert(connections::kMaxGroupLen < MAX_LINE_CHARS, "a group name must fit the line buffers");
static_assert(MAX_LINE_CHARS <= 255, "Lines offsets are uint8_t");

// One string cut into lines, as offsets into it. A break eats the space it broke at.
struct Lines {
  int count = 0;
  uint8_t begin[MAX_LINES] = {};
  uint8_t end[MAX_LINES] = {};
};

// The width of text[begin, end) in this cut. A span too long to measure answers wider
// than anything, never "fits".
int spanWidth(Font font, const char* text, int begin, int end);

// Cuts text so every line measures inside width, breaking at the last space in reach.
// Returns false when it needs more than maxLines, or would have to cut a word and
// allowMidWord is false. lines still holds what was laid out.
bool breakToFit(Font font, const char* text, int width, int maxLines, bool allowMidWord, bool preferSpaces, Lines& lines);

// Evens out a break that had to cut a word, so "MISCREAN / T" becomes "MISCR / EANT".
// Only applied where every even share is measured to fit.
void balanceBreaks(Font font, const char* text, int width, int maxLines, Lines& lines);

struct TileCut {
  Font font = CUTS[0];
  int maxLines = 1;
  bool mayBreakWords = false;  // true only when no cut sets every word whole
};

// The one cut for every tile on the board.
TileCut chooseTileCut(const char* const* words, int count, int innerWidth, int innerHeight);

// One tile's lines in the board's cut.
void layoutTile(const char* word, int innerWidth, const TileCut& cut, Lines& lines);

// A solved group: its name over its four words, in the largest pair of cuts that fits
// the row, the name keeping its size ahead of the list.
struct SolvedRow {
  char words[MAX_LINE_CHARS] = {};  // "A, B, C, D"
  Font nameFont = CUTS[CUT_COUNT - 1];
  Font listFont = CUTS[CUT_COUNT - 1];
  Lines name;
  Lines list;
  int nameHeight = 0;
  int listHeight = 0;
  bool fits = false;  // false only if the pair is taller than the row even at the smallest cut
};

void layoutSolvedRow(const connections::Group& group, int width, int height, SolvedRow& row);

}  // namespace connectionsfit
