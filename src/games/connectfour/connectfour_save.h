#pragma once

#include <cstddef>
#include <cstdint>

#include "connectfour_rules.h"

// Connect Four's saves as bytes. No screen or card code, so tests/connectfour/ can
// check them. The board is 42 bytes, column by column from the bottom, so cell
// (c, r) is byte c * 6 + r: 0 empty, 1 light (the outlined discs), 2 dark.
namespace connectfour {

// The game in play, in the save named "connectfour", erased once it's settled. You
// always play light, which opens, against the built-in opponent.
//
//   [0..41]  the board
//   [42]     the last disc's column, or 0xFF before the first
//   [43]     the last disc's row, or 0xFF
//   [44]     whose turn: 1 light, 2 dark
constexpr uint8_t GAME_VERSION = 1;
constexpr size_t GAME_BYTES = 45;

void packGame(const Game& game, uint8_t* out);

// False for anything a game still being played can't hold: a wrong size, a cell
// other than 0-2, a disc above a gap, disc counts or a turn play can't reach, four
// in a line, a full board, or a last disc that isn't the top of its column or
// isn't the side that just moved.
bool unpackGame(const uint8_t* data, size_t size, Game& game);

// The record, in the save named "connectfour-record", as CrossPlay keeps it: the
// tally, and the board the last game ended on with its winning line, drawn on the
// menu.
//
//   [0..1]    wins
//   [2..3]    losses
//   [4..5]    draws
//   [6]       1 once a game has ended, so what follows means something
//   [7]       how the last game ended: 0 you won, 1 they won, 2 a draw
//   [8..11]   the winning line's four cells (c * 6 + r), or 0xFF for a draw
//   [12..53]  the last board
constexpr uint8_t RECORD_VERSION = 1;
constexpr size_t RECORD_BYTES = 54;

constexpr uint8_t LAST_WON = 0;
constexpr uint8_t LAST_LOST = 1;
constexpr uint8_t LAST_DRAW = 2;

struct Record {
  int wins = 0;
  int losses = 0;
  int draws = 0;
  bool hasLast = false;
  uint8_t lastOutcome = LAST_DRAW;
  int lastLine[kLine] = {-1, -1, -1, -1};
  uint8_t lastCells[kCells] = {};
};

void packRecord(const Record& record, uint8_t* out);

// False for a wrong size, a cell other than 0-2, a disc above a gap, a win or loss
// whose line isn't four of that side's discs in a row, a draw on a board that
// isn't full or with a line, or a last board where none is claimed.
bool unpackRecord(const uint8_t* data, size_t size, Record& record);

}  // namespace connectfour
