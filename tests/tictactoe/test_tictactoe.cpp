// Tic Tac Toe's rules.
//
// The game is small enough to play every game there is, so the main test does:
// the published counts are the whole-tree check, and a rule that is wrong
// anywhere in the tree moves them.

#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "tictactoe_rules.h"

using namespace tictactoe;

namespace {

int checks = 0;
int failures = 0;

#define CHECK(cond)                                               \
  do {                                                            \
    ++checks;                                                     \
    if (!(cond)) {                                                \
      ++failures;                                                 \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
    }                                                             \
  } while (0)

// A board from a picture, top row first: '.', 'X' or 'O'. Whatever the picture
// holds, the game is left running with `turn` to move.
Game position(const char* cells, const uint8_t turn) {
  Game game;
  start(game);
  for (int i = 0; i < kCells; ++i) game.cell[i] = cells[i] == 'X' ? kX : (cells[i] == 'O' ? kO : kEmpty);
  game.turn = turn;
  return game;
}

void testTheOpeningBoardIsEmptyAndXOpens() {
  Game game;
  std::memset(&game, 0xAB, sizeof(game));
  start(game);
  for (const uint8_t cell : game.cell) CHECK(cell == kEmpty);
  CHECK(game.turn == kX);
  CHECK(game.outcome == Outcome::Running);
  CHECK(!over(game));
}

void testPlayersAlternate() {
  Game game;
  start(game);
  CHECK(place(game, 4));
  CHECK(game.cell[4] == kX);
  CHECK(game.turn == kO);
  CHECK(place(game, 0));
  CHECK(game.cell[0] == kO);
  CHECK(game.turn == kX);
}

void testARefusedMoveChangesNothing() {
  Game game;
  start(game);
  CHECK(place(game, 4));

  const int refused[] = {4, -1, kCells, 100};
  for (const int cell : refused) {
    const Game before = game;
    CHECK(!canPlace(game, cell));
    CHECK(!place(game, cell));
    CHECK(std::memcmp(&before, &game, sizeof(Game)) == 0);
  }
}

void testEveryLineWinsForBothMarks() {
  CHECK(sizeof(kLines) / sizeof(kLines[0]) == kLineCount);
  for (const auto& line : kLines) {
    for (const uint8_t side : {kX, kO}) {
      Game game;
      start(game);
      game.turn = side;
      game.cell[line[0]] = side;
      game.cell[line[1]] = side;
      CHECK(place(game, line[2]));
      CHECK(game.outcome == (side == kX ? Outcome::XWins : Outcome::OWins));
      CHECK(over(game));

      int found[kLine] = {-1, -1, -1};
      CHECK(winningLine(game, side, found));
      for (int i = 0; i < kLine; ++i) CHECK(found[i] == line[i]);
      CHECK(!winningLine(game, other(side), nullptr));
    }
  }
}

void testTwoInALineAndABrokenLineDoNotWin() {
  Game game = position("XX.OXO.O.", kO);
  CHECK(!winningLine(game, kX, nullptr));
  CHECK(!winningLine(game, kO, nullptr));
  int untouched[kLine] = {-7, -7, -7};
  CHECK(!winningLine(game, kX, untouched));
  for (const int value : untouched) CHECK(value == -7);
}

void testAWinEndsTheGameAndKeepsTheTurn() {
  Game game = position("XX.OO....", kX);
  CHECK(place(game, 2));
  CHECK(game.outcome == Outcome::XWins);
  CHECK(game.turn == kX);

  const Game finished = game;
  for (int cell = 0; cell < kCells; ++cell) {
    CHECK(!canPlace(game, cell));
    CHECK(!place(game, cell));
  }
  CHECK(std::memcmp(&finished, &game, sizeof(Game)) == 0);
}

void testAFullBoardWithNoLineIsADraw() {
  Game game = position("XOXXOOOX.", kX);
  CHECK(place(game, 8));
  CHECK(game.outcome == Outcome::Draw);
  CHECK(!winningLine(game, kX, nullptr));
  CHECK(!winningLine(game, kO, nullptr));

  const Game finished = game;
  CHECK(!place(game, 8));
  CHECK(std::memcmp(&finished, &game, sizeof(Game)) == 0);
}

void testAWinOnTheNinthMarkIsAWinNotADraw() {
  Game game = position("XOXOXOOX.", kX);
  CHECK(place(game, 8));
  CHECK(boardFull(game));
  CHECK(game.outcome == Outcome::XWins);
}

void testStartClearsAFinishedGame() {
  Game game = position("XX.OO....", kX);
  CHECK(place(game, 2));
  start(game);
  for (const uint8_t cell : game.cell) CHECK(cell == kEmpty);
  CHECK(game.turn == kX);
  CHECK(game.outcome == Outcome::Running);
}

struct Tally {
  long xWins = 0;
  long oWins = 0;
  long draws = 0;
};

void playEveryGame(const Game& game, const int marks, Tally& tally) {
  if (over(game)) {
    if (game.outcome == Outcome::XWins) ++tally.xWins;
    if (game.outcome == Outcome::OWins) ++tally.oWins;
    if (game.outcome == Outcome::Draw) ++tally.draws;
    return;
  }
  // A running game always has a move, and the marks always differ by the turn.
  CHECK(marks < kCells);
  CHECK(game.turn == (marks % 2 == 0 ? kX : kO));
  for (int cell = 0; cell < kCells; ++cell) {
    Game next = game;
    if (!place(next, cell)) continue;
    playEveryGame(next, marks + 1, tally);
  }
}

// The published counts for the whole game tree.
void testEveryPossibleGameEndsAsPublished() {
  Game game;
  start(game);
  Tally tally;
  playEveryGame(game, 0, tally);
  CHECK(tally.xWins == 131184);
  CHECK(tally.oWins == 77904);
  CHECK(tally.draws == 46080);
  CHECK(tally.xWins + tally.oWins + tally.draws == 255168);
}

}  // namespace

int main() {
  testTheOpeningBoardIsEmptyAndXOpens();
  testPlayersAlternate();
  testARefusedMoveChangesNothing();
  testEveryLineWinsForBothMarks();
  testTwoInALineAndABrokenLineDoNotWin();
  testAWinEndsTheGameAndKeepsTheTurn();
  testAFullBoardWithNoLineIsADraw();
  testAWinOnTheNinthMarkIsAWinNotADraw();
  testStartClearsAFinishedGame();
  testEveryPossibleGameEndsAsPublished();

  std::printf("%d checks, %d failed\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
