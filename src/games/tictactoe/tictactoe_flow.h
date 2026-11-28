#pragma once

// When a new game has to be confirmed, and whether a board from the other device can
// be trusted. Freestanding, so it's tested on the PC.

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

// Whether `after` is `before` with one more legal move, made by `mover`. A board from
// the other device is only used if it is, so a damaged or foreign packet can't put
// an impossible board on screen.
inline bool isNextMove(const Game& before, const Game& after, uint8_t mover) {
  if (over(before) || before.turn != mover) return false;
  for (int cell = 0; cell < kCells; cell++) {
    Game next = before;
    if (!place(next, cell)) continue;
    bool same = next.turn == after.turn && next.outcome == after.outcome;
    for (int i = 0; same && i < kCells; i++) same = next.cell[i] == after.cell[i];
    if (same) return true;
  }
  return false;
}

// Who made the last move of a finished game: the winner, or X after a draw, since a
// full board holds five X and four O.
inline uint8_t lastMover(const Game& game) { return game.outcome == Outcome::OWins ? kO : kX; }

// Your mark in a game between two devices. The first game takes it from the coin
// toss. The link doesn't reset the turn for another game, so whoever didn't make the
// last move goes next, and plays X because X opens: the loser opens, and O after a
// draw.
inline uint8_t nextMark(const Game& previous, uint8_t previousMine, bool goesFirst) {
  if (!over(previous)) return goesFirst ? kX : kO;
  return lastMover(previous) == previousMine ? kO : kX;
}

}  // namespace tictactoe
