#pragma once

#include <cstddef>
#include <cstdint>

#include "solitaire_rules.h"

// The board in play as bytes, for the save named "solitaire". No screen or card
// code, so tests/solitaire/ can check it.
//
//   [0..3]    seed
//   [4]       1 for draw three, 0 for draw one
//   [5..6]    moves made
//   [7..19]   the thirteen piles' card counts, in pile order
//   [20..71]  the 52 cards, pile by pile from the bottom, as cards.h encodes them
//   [72]      undo steps kept, 0..96
//   then two bytes per step, oldest first: from << 4 | to, then the count, with
//             0x20 for a turned card and 0x40 for a stock deal
namespace solitaire {

constexpr uint8_t SAVE_VERSION = 1;
constexpr size_t SAVE_HEADER_BYTES = 73;
constexpr size_t UNDO_STEP_BYTES = 2;
constexpr size_t MAX_SAVE_BYTES = SAVE_HEADER_BYTES + kUndoDepth * UNDO_STEP_BYTES;  // 265

// Writes the game into out, which holds MAX_SAVE_BYTES, and returns how many it used.
size_t packGame(const Game& game, uint8_t* out);

// Rebuilds a game, undo history included. False, with game untouched, for anything
// that isn't a board this game could reach: a wrong size, a count or card out of
// range, a card face the wrong way for its pile, a deck that isn't 52 distinct
// cards, or an undo step that doesn't fit.
bool unpackGame(const uint8_t* data, size_t size, Game& game);

}  // namespace solitaire
