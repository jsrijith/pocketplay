// When Tic Tac Toe asks before starting a new game, and which boards from the other
// device it takes.

#include <cstdio>
#include <initializer_list>

#include "tictactoe_flow.h"

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

void testAnEmptyBoardStartsAtOnce() {
  Game game;
  start(game);
  CHECK(!worthKeeping(game, kEmpty));
  CHECK(!worthKeeping(game, kX));
  CHECK(!worthKeeping(game, kO));
}

void testWithTwoPlayersAnyMarkAsks() {
  Game game;
  start(game);
  place(game, 4);
  CHECK(worthKeeping(game, kEmpty));
}

// Only the computer has moved, so there's nothing of yours to lose.
void testTheComputersOpeningAloneDoesNotAsk() {
  Game game;
  start(game);
  place(game, 4);  // the computer, playing X
  CHECK(!worthKeeping(game, kX));
  place(game, 0);  // you, as O
  CHECK(worthKeeping(game, kX));
}

void testAYourMarkAgainstTheComputerAsks() {
  Game game;
  start(game);
  place(game, 4);  // you, as X
  CHECK(worthKeeping(game, kO));
}

void testAFinishedGameStartsAtOnce() {
  Game game;
  start(game);
  for (const int cell : {0, 3, 1, 4, 2}) place(game, cell);  // X takes the top row
  CHECK(over(game));
  CHECK(!worthKeeping(game, kEmpty));
  CHECK(!worthKeeping(game, kO));
}

// A board from the other device is used only if it's yours with one move of theirs.
void testTheirNextMoveIsAccepted() {
  Game before;
  start(before);
  place(before, 4);  // you, as X
  Game after = before;
  place(after, 0);  // them, as O
  CHECK(isNextMove(before, after, kO));
}

void testAnythingElseIsRefused() {
  Game before;
  start(before);
  place(before, 4);  // you, as X: O to move

  Game same = before;
  CHECK(!isNextMove(before, same, kO));  // nothing new

  Game two = before;
  place(two, 0);
  place(two, 8);
  CHECK(!isNextMove(before, two, kO));  // two moves

  Game wrongMark = before;
  wrongMark.cell[0] = kX;  // an X where O should be, and the turn not passed
  CHECK(!isNextMove(before, wrongMark, kO));

  Game moved = before;
  moved.cell[4] = kEmpty;
  moved.cell[0] = kO;
  moved.turn = kX;
  CHECK(!isNextMove(before, moved, kO));  // your mark taken away

  Game after = before;
  place(after, 0);
  CHECK(!isNextMove(before, after, kX));  // it isn't X's move to make

  Game badOutcome = after;
  badOutcome.outcome = Outcome::OWins;
  CHECK(!isNextMove(before, badOutcome, kO));  // a win that isn't on the board
}

void testTheWinningMoveIsAccepted() {
  Game before;
  start(before);
  for (const int cell : {0, 3, 1, 4}) place(before, cell);  // X to move, two in the top row
  Game after = before;
  place(after, 2);
  CHECK(after.outcome == Outcome::XWins);
  CHECK(isNextMove(before, after, kX));
  CHECK(!isNextMove(after, after, kO));  // nothing follows a finished game
}

void testTheMarkForEachGame() {
  Game fresh;
  start(fresh);
  CHECK(nextMark(fresh, kX, true) == kX);  // the first game: the coin toss
  CHECK(nextMark(fresh, kX, false) == kO);

  Game xWon;
  start(xWon);
  for (const int cell : {0, 3, 1, 4, 2}) place(xWon, cell);
  CHECK(lastMover(xWon) == kX);
  CHECK(nextMark(xWon, kX, true) == kO);  // you won as X, so they open
  CHECK(nextMark(xWon, kO, true) == kX);  // you lost as O, so you open

  Game drawn;
  start(drawn);
  for (const int cell : {0, 1, 2, 4, 3, 5, 7, 6, 8}) place(drawn, cell);
  CHECK(drawn.outcome == Outcome::Draw);
  CHECK(nextMark(drawn, kX, false) == kO);
  CHECK(nextMark(drawn, kO, false) == kX);  // O opens after a draw
}

}  // namespace

int main() {
  testAnEmptyBoardStartsAtOnce();
  testWithTwoPlayersAnyMarkAsks();
  testTheComputersOpeningAloneDoesNotAsk();
  testAYourMarkAgainstTheComputerAsks();
  testAFinishedGameStartsAtOnce();
  testTheirNextMoveIsAccepted();
  testAnythingElseIsRefused();
  testTheWinningMoveIsAccepted();
  testTheMarkForEachGame();

  std::printf("%d checks, %d failed\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
