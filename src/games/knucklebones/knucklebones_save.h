#pragma once

#include <cstddef>
#include <cstdint>

#include "knucklebones_rules.h"

// Knucklebones' saves as bytes. No screen or card code, so tests/knucklebones/ can
// check them. A grid is nine bytes, column by column from the near end, so cell
// (c, r) is byte c * 3 + r, and 0 is an empty slot.
namespace knucklebones {

// The match in play, in the save named "knucklebones", erased once it ends. You
// always play seat 0 against the built-in opponent.
//
//   [0..8]    your grid
//   [9..17]   the opponent's grid
//   [18]      whose turn: 0 yours, 1 theirs
//   [19]      the die waiting to be placed, 1-6
//   [20..23]  the random state the next dice come from
constexpr uint8_t MATCH_VERSION = 1;
constexpr size_t MATCH_BYTES = 24;

void packMatch(const Game& game, uint8_t* out);

// False for anything a match still being played can't hold: a wrong size, a die
// face past 6, a die above a gap, a turn other than 0 or 1, no die waiting, a
// random state of zero, or a full grid (which would have ended it).
bool unpackMatch(const uint8_t* data, size_t size, Game& game);

// The record, in the save named "knucklebones-record", as CrossPlay keeps it: the
// tally, and the grids the last match ended on, drawn on the menu.
//
//   [0..1]    wins
//   [2..3]    losses
//   [4..5]    draws
//   [6]       1 once a match has ended, so the grids below mean something
//   [7..15]   your last grid
//   [16..24]  the opponent's last grid
constexpr uint8_t RECORD_VERSION = 1;
constexpr size_t RECORD_BYTES = 25;

struct Record {
  int wins = 0;
  int losses = 0;
  int draws = 0;
  bool hasLast = false;
  Grid lastYours{};
  Grid lastTheirs{};
};

void packRecord(const Record& record, uint8_t* out);

// False for a wrong size, a die face past 6, a die above a gap, grids without
// a finished match behind them (neither one full), or grids where none is claimed.
bool unpackRecord(const uint8_t* data, size_t size, Record& record);

}  // namespace knucklebones
