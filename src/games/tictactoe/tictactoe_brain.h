#pragma once

// The computer opponent. Freestanding: no screen, no card, and its randomness comes
// in as a seed, so the PC tests can play every game it could.
//
// HARD searches the whole game (minimax). Tic Tac Toe has fewer than 6,000 positions,
// so this takes well under a millisecond on the ESP32, and it never loses. Of the
// moves that are equally good it picks one at random, so it doesn't play the same
// game every time.
//
// EASY takes a win when it has one, blocks yours only half the time, and otherwise
// plays any empty square. It can be beaten, but it doesn't hand you the game.

#include <cstdint>

#include "tictactoe_rules.h"

namespace tictactoe {

enum class Level : uint8_t { Easy, Hard };

// xorshift32: small, and good enough to choose a square. Never seed it with 0.
struct Rng {
  uint32_t state = 0x9E3779B9u;

  uint32_t next() {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
  }
  int below(int count) { return static_cast<int>(next() % static_cast<uint32_t>(count)); }
};

// A finished position from `side`'s point of view: a win is worth more the sooner it
// comes, and a loss less the later, so HARD wins quickly and holds out longest.
// `empties` is how many squares were still empty when it finished.
inline int scoreFor(const Game& game, uint8_t side, int empties) {
  if (game.outcome == Outcome::Draw) return 0;
  const bool sideWon = (game.outcome == Outcome::XWins) == (side == kX);
  return sideWon ? 1 + empties : -1 - empties;
}

inline int emptyCount(const Game& game) {
  int count = 0;
  for (const uint8_t cell : game.cell) {
    if (cell == kEmpty) count++;
  }
  return count;
}

// The value of a running game for the player to move, with best play by both.
inline int searchValue(const Game& game) {
  int best = -100;
  for (int cell = 0; cell < kCells; cell++) {
    Game next = game;
    if (!place(next, cell)) continue;
    const int value = over(next) ? scoreFor(next, game.turn, emptyCount(next)) : -searchValue(next);
    if (value > best) best = value;
  }
  return best;
}

// Every move that's as good as any other for the player to move. Returns how many it
// wrote to `out`, which holds kCells, and 0 only for a finished game.
inline int bestCells(const Game& game, int* out) {
  int count = 0;
  int best = -100;
  for (int cell = 0; cell < kCells; cell++) {
    Game next = game;
    if (!place(next, cell)) continue;
    const int value = over(next) ? scoreFor(next, game.turn, emptyCount(next)) : -searchValue(next);
    if (value > best) {
      best = value;
      count = 0;
    }
    if (value == best) out[count++] = cell;
  }
  return count;
}

// A square that completes a line for `side` right now, or -1.
inline int winningCell(const Game& game, uint8_t side) {
  for (int cell = 0; cell < kCells; cell++) {
    if (!canPlace(game, cell)) continue;
    Game next = game;
    next.cell[cell] = side;
    if (winningLine(next, side, nullptr)) return cell;
  }
  return -1;
}

// The computer's square for the player to move, or -1 for a finished game.
inline int chooseCell(const Game& game, Level level, Rng& rng) {
  if (over(game)) return -1;

  if (level == Level::Hard) {
    int cells[kCells];
    const int count = bestCells(game, cells);
    return count == 0 ? -1 : cells[rng.below(count)];
  }

  const int win = winningCell(game, game.turn);
  if (win >= 0) return win;
  const int block = winningCell(game, other(game.turn));
  const bool blocks = rng.below(2) == 0;
  if (block >= 0 && blocks) return block;

  int empty[kCells];
  int count = 0;
  for (int cell = 0; cell < kCells; cell++) {
    if (canPlace(game, cell)) empty[count++] = cell;
  }
  return count == 0 ? -1 : empty[rng.below(count)];
}

}  // namespace tictactoe
