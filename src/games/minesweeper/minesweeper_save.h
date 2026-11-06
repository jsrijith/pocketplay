#pragma once

#include <cstddef>
#include <cstdint>

#include "minesweeper_rules.h"

// Minesweeper's saves as bytes. No screen or card code, so tests/minesweeper/ can
// check them. A cell is its rules byte (mine, revealed and flagged bits), column by
// column, so cell (c, r) is byte c * 10 + r.
namespace minesweeper {

// The board in play, in the save named "minesweeper", erased once it's settled.
//
//   [0..79]   the cells
//   [80..83]  the random state the mines are laid from
//   [84]      0 untouched (no mines yet), 1 playing
//   [85]      1 while FLAG is what a tap does
constexpr uint8_t BOARD_VERSION = 1;
constexpr size_t BOARD_BYTES = 86;

struct OpenBoard {
  Game game{};
  bool flagMode = false;
};

void packBoard(const OpenBoard& board, uint8_t* out);

// False for anything this game can't reach while still playing: a wrong size, a
// cell with a stray bit, an untouched board with anything on it, the wrong number
// of mines, a dug mine, a flag on an open cell, nothing dug, or every safe cell
// open (which would be a win).
bool unpackBoard(const uint8_t* data, size_t size, OpenBoard& board);

// The record, in the save named "minesweeper-history", as CrossPlay keeps it: wins,
// losses, and the last settled board, drawn on the menu.
//
//   [0..1]    wins
//   [2..3]    losses
//   [4..83]   the last board's cells
constexpr uint8_t HISTORY_VERSION = 1;
constexpr size_t HISTORY_BYTES = 84;

struct History {
  int wins = 0;
  int losses = 0;
  Game lastBoard{};  // only its cells mean anything
};

void packHistory(const History& history, uint8_t* out);

// False for a wrong size, a cell with a stray bit, or a board without ten mines.
bool unpackHistory(const uint8_t* data, size_t size, History& history);

// The settled board lost: a mine was dug.
bool boardLost(const Game& board);

}  // namespace minesweeper
