// Tic Tac Toe played between two devices over FakeLink (tests/link/), as the game
// plays it nearby: the whole board is sent after each move, and a board that
// arrives is kept only if it's one move of theirs on. Each side's moves come from
// the computer player. Both devices must end on the same board, once each.
#include <cstdio>
#include <cstring>

#include "FakeLink.h"
#include "core/link/match.h"
#include "tictactoe_brain.h"
#include "tictactoe_flow.h"

namespace {

using namespace linkplay;
using namespace linktest;
namespace ttt = tictactoe;

int checks = 0;
int failed = 0;

void check(bool ok, const char* what, int line) {
  checks++;
  if (ok) return;
  failed++;
  printf("FAIL  line %d: %s\n", line, what);
}
#define CHECK(expr) check((expr), #expr, __LINE__)

struct Device final : MatchHost {
  Device(Medium& medium, uint8_t last, uint32_t seed, ttt::Level level)
      : link(medium, addressOf(last)), play(&link), match(play, *this), level(level) {
    rng.state = seed;
    ttt::start(game);
  }

  void onMatchStart(bool goesFirst) override {
    mine = ttt::nextMark(game, mine, goesFirst);
    ttt::start(game);
    starts++;
  }
  bool takeOpponentState() override {
    ttt::Game incoming{};
    if (!play.takeOpponent(incoming)) return false;
    if (!ttt::isNextMove(game, incoming, ttt::other(mine))) {
      refused++;
      return false;
    }
    game = incoming;
    return true;
  }
  bool matchGameOver() const override { return ttt::over(game); }
  void onMatchEnded() override { ended++; }
  void onLinkEnded() override {}

  // A tap on the board, as the game makes it: only on your turn, and only kept if
  // the link takes it.
  void act() {
    if (!match.yourTurn() || ttt::over(game) || game.turn != mine) return;
    ttt::Game next = game;
    if (!ttt::place(next, ttt::chooseCell(game, level, rng))) return;
    if (play.play(next)) game = next;
  }

  FakeTransport link;
  Play<ttt::Game> play;
  Match match;
  ttt::Rng rng;
  ttt::Level level;
  ttt::Game game{};
  uint8_t mine = ttt::kX;
  int starts = 0;
  int ended = 0;
  int refused = 0;
};

bool sameGame(const ttt::Game& a, const ttt::Game& b) {
  return memcmp(a.cell, b.cell, sizeof(a.cell)) == 0 && a.turn == b.turn && a.outcome == b.outcome;
}

void run(Medium& medium, Device& a, Device& b, int passes) {
  for (int pass = 0; pass < passes; pass++) {
    medium.nowMs += 15;
    a.match.update(medium.nowMs);
    b.match.update(medium.nowMs);
    if (pass % 4 == 0) {
      a.act();
      b.act();
    }
    medium.collect();
  }
}

void playMatch(int loss, int duplicate, int jitter, uint32_t seed, ttt::Level level) {
  Medium medium;
  medium.lossPercent = loss;
  medium.duplicatePercent = duplicate;
  medium.maxJitterMs = jitter;
  medium.random = seed;
  Device a(medium, 1, seed * 7 + 1, level);
  Device b(medium, 2, seed * 13 + 5, ttt::Level::Easy);
  CHECK(a.match.enter(GameId::TicTacToe, "A") && b.match.enter(GameId::TicTacToe, "B"));

  for (int pass = 0; pass < 20000 && !(ttt::over(a.game) && ttt::over(b.game)); pass++) run(medium, a, b, 1);
  // A little longer, for the last acknowledgement and the endgame.
  run(medium, a, b, 400);

  CHECK(a.starts == 1 && b.starts == 1);
  CHECK(a.mine != b.mine);
  CHECK(ttt::over(a.game) && ttt::over(b.game));
  CHECK(sameGame(a.game, b.game));
  CHECK(a.ended == 1 && b.ended == 1);
  // Over a link that duplicates and reorders, the protocol still hands each board
  // over once, so nothing honest is ever refused.
  CHECK(a.refused == 0 && b.refused == 0);
}

// Another game: both say yes, and the second game starts from an empty board and
// also ends the same on both.
void playARematch() {
  Medium medium;
  medium.random = 99;
  Device a(medium, 1, 11, ttt::Level::Hard);
  Device b(medium, 2, 23, ttt::Level::Hard);
  CHECK(a.match.enter(GameId::TicTacToe, "A") && b.match.enter(GameId::TicTacToe, "B"));
  for (int pass = 0; pass < 20000 && !(ttt::over(a.game) && ttt::over(b.game)); pass++) run(medium, a, b, 1);
  run(medium, a, b, 400);
  CHECK(a.ended == 1 && b.ended == 1);
  CHECK(a.game.outcome == ttt::Outcome::Draw);  // HARD against HARD

  // One asks, and the other answers a moment later, as two people would.
  a.match.proposeRematch();
  run(medium, a, b, 100);
  CHECK(a.starts == 1 && b.starts == 1);  // B hasn't answered
  b.match.proposeRematch();
  for (int pass = 0; pass < 20000 && !(a.starts == 2 && b.starts == 2 && ttt::over(a.game) && ttt::over(b.game)); pass++) {
    run(medium, a, b, 1);
  }
  run(medium, a, b, 400);
  CHECK(a.starts == 2 && b.starts == 2);
  CHECK(ttt::over(a.game) && sameGame(a.game, b.game));
  CHECK(a.ended == 2 && b.ended == 2);
  CHECK(a.mine != b.mine);
}

// After a win the loser opens the next game, as X, and the link agrees it's their turn.
void theLoserOpensTheNext() {
  Medium medium;
  medium.random = 5;
  Device a(medium, 1, 3, ttt::Level::Hard);
  Device b(medium, 2, 4, ttt::Level::Easy);
  CHECK(a.match.enter(GameId::TicTacToe, "A") && b.match.enter(GameId::TicTacToe, "B"));
  int decided = 0;
  for (int game = 0; game < 6; game++) {
    for (int pass = 0; pass < 20000 && !(ttt::over(a.game) && ttt::over(b.game)); pass++) run(medium, a, b, 1);
    run(medium, a, b, 400);
    CHECK(sameGame(a.game, b.game));
    const ttt::Outcome outcome = a.game.outcome;
    const uint8_t winner = outcome == ttt::Outcome::XWins ? ttt::kX : ttt::kO;
    Device& loser = a.mine == winner ? b : a;
    const bool drawn = outcome == ttt::Outcome::Draw;
    Device& oPlayer = a.mine == ttt::kO ? a : b;

    a.match.proposeRematch();
    run(medium, a, b, 100);
    b.match.proposeRematch();
    run(medium, a, b, 2);
    CHECK(a.starts == game + 2 && b.starts == game + 2);
    if (drawn) {
      CHECK(oPlayer.mine == ttt::kX);  // after a draw, O opens the next
    } else {
      decided++;
      CHECK(loser.mine == ttt::kX);
    }
  }
  CHECK(decided > 0);  // HARD against EASY: some games are won
}

}  // namespace

int main() {
  for (uint32_t seed = 1; seed <= 5; seed++) playMatch(0, 0, 0, seed, ttt::Level::Easy);
  for (uint32_t seed = 1; seed <= 5; seed++) playMatch(25, 15, 50, seed, ttt::Level::Easy);
  for (uint32_t seed = 6; seed <= 8; seed++) playMatch(25, 15, 50, seed, ttt::Level::Hard);
  playARematch();
  theLoserOpensTheNext();
  printf("tictactoe nearby: %d checks, %d failed\n", checks, failed);
  return failed == 0 ? 0 : 1;
}
