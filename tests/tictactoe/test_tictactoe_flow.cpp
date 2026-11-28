// When Tic Tac Toe asks before starting a new game.

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

}  // namespace

int main() {
  testAnEmptyBoardStartsAtOnce();
  testWithTwoPlayersAnyMarkAsks();
  testTheComputersOpeningAloneDoesNotAsk();
  testAYourMarkAgainstTheComputerAsks();
  testAFinishedGameStartsAtOnce();

  std::printf("%d checks, %d failed\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
