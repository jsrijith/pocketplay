#pragma once

#include <cstddef>
#include <cstdint>

#include "connections_rules.h"

// The board in hand as bytes, with no screen or card, so it can be tested off the
// device. Packed byte by byte, least significant first:
//
//   [0..1]   the puzzle's position in the pack
//   [2..5]   its date, YYYYMMDD, so a re-downloaded pack can't hand the save a
//            different puzzle at the same position
//   [6..9]   the shuffle seed
//   [10]     groups solved, 0..3
//   [11..14] which groups, in the order found
//   [15]     mistakes, 0..3
//   [16]     guesses tried, 0..8
//   [17..32] those guesses, two bytes each, as masks over the puzzle's sixteen words
//   [33..34] the words selected, as a mask over the puzzle's sixteen words, so a
//            board dealt in another order still selects the same words
//
// Only a game still being played is saved. Finished ones are in the results blob.
namespace connections {

constexpr uint8_t SAVE_VERSION = 1;
constexpr size_t SAVE_BYTES = 35;
constexpr int MAX_GUESSES = 8;

struct OpenBoard {
  int index = 0;
  uint32_t date = 0;
  Game::Save state;
  uint16_t selected = 0;  // bit group * 4 + member
};

size_t packBoard(const OpenBoard& board, uint8_t* out);

// Returns false and leaves board alone unless every field is in range: a running
// game, distinct solved groups, each guess exactly four words, and at most four
// words selected, none of them already solved. Game::restore()
// then checks it against the puzzle itself.
bool unpackBoard(uint8_t version, const uint8_t* in, size_t size, OpenBoard& board);

}  // namespace connections
