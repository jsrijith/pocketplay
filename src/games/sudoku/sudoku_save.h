#pragma once

#include <cstddef>
#include <cstdint>

#include "sudoku_game.h"

// Sudoku's saves as bytes. No screen or card code, so tests/sudoku/ can check them.
namespace sudoku {

// The game, in the save named "sudoku". As in CrossPlay the answer isn't saved:
// the clues have exactly one solution, so it's solved again on load, and the level
// and hardest technique are graded again too.
//
//   [0]        the menu's level, 0-3
//   [1]        1 once a puzzle has been started
//   [2]        the armed digit, 1-9
//   [3]        the hinted cell, or 81 for none
//   [4]        the hinted digit, 0-9
//   [5]        the hinted technique, 0-10
//   [6]        hints used
//   [7]        1 once solved
//   [8..11]    milliseconds spent solving
//   [12]       undo steps held, 0-24
//   [13]       where the next undo step goes, 0-23
//   [14..94]   each cell: 0x10 | the clue, or the player's digit (0 for empty)
//   [95..256]  each cell's pencil marks, two bytes, bit d-1 for digit d
//   [257..352] the 24 undo slots: cell, digit before, marks before (two bytes)
constexpr uint8_t SAVE_VERSION = 1;
constexpr size_t SAVE_BYTES = 353;

struct Saved {
  Game game{};
  bool hasGame = false;
  Level menuLevel = Level::Easy;
};

void packGame(const Saved& saved, uint8_t* out);

// Rebuilds the game, solving the clues for the answer. False, with saved untouched,
// for a wrong size or any field out of range, clues without exactly one solution
// the solver can reach, a digit or mark where the game can't put one, a solved flag
// on a board that isn't solved, or an undo step on a clue.
bool unpackGame(const uint8_t* data, size_t size, Saved& saved);

// The record, in the save named "sudoku-record": solves and best unaided times per
// level, and hints taken.
//
//   [0..7]    solves at each level, two bytes each
//   [8..23]   best time at each level in milliseconds, four bytes each, 0 for none
//   [24..25]  hints taken
constexpr uint8_t RECORD_VERSION = 1;
constexpr size_t RECORD_BYTES = 26;

void packRecord(const Record& record, uint8_t* out);
bool unpackRecord(const uint8_t* data, size_t size, Record& record);

}  // namespace sudoku
