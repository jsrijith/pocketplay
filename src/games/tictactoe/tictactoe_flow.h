#pragma once

// When a new game has to be confirmed. Freestanding, so it's tested on the PC.

#include <cstdint>

#include "tictactoe_rules.h"

namespace tictactoe {

// A running game with a mark of yours in it, which a stray tap on a mode or NEW
// GAME mustn't throw away. With two players (`computer` is kEmpty) any mark counts,
// and against the computer only one that isn't its own: a game where only the
// computer has opened has nothing to lose.
inline bool worthKeeping(const Game& game, uint8_t computer) {
  if (over(game)) return false;
  for (const uint8_t cell : game.cell) {
    if (cell != kEmpty && cell != computer) return true;
  }
  return false;
}

}  // namespace tictactoe
