// Tic Tac Toe's computer player. The game is small enough to search all of it, so
// the HARD test plays every game a HARD player could choose against every reply.

#include <cstdio>
#include <initializer_list>

#include "tictactoe_brain.h"

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

Game position(const char* cells, const uint8_t turn) {
  Game game;
  start(game);
  for (int i = 0; i < kCells; ++i) game.cell[i] = cells[i] == 'X' ? kX : (cells[i] == 'O' ? kO : kEmpty);
  game.turn = turn;
  return game;
}

// HARD's side takes every move bestCells() allows, and the other side every legal
// move. Counts each way a game ends, from HARD's point of view.
struct Tally {
  long wins = 0;
  long draws = 0;
  long losses = 0;
};

void playHardAgainstEverything(const Game& game, uint8_t hard, Tally& tally) {
  if (over(game)) {
    if (game.outcome == Outcome::Draw) {
      tally.draws++;
    } else if ((game.outcome == Outcome::XWins) == (hard == kX)) {
      tally.wins++;
    } else {
      tally.losses++;
    }
    return;
  }
  int cells[kCells];
  int count = 0;
  if (game.turn == hard) {
    count = bestCells(game, cells);
  } else {
    for (int cell = 0; cell < kCells; cell++) {
      if (canPlace(game, cell)) cells[count++] = cell;
    }
  }
  CHECK(count > 0);
  for (int i = 0; i < count; i++) {
    Game next = game;
    CHECK(place(next, cells[i]));
    playHardAgainstEverything(next, hard, tally);
  }
}

void testHardNeverLosesAsEitherMark() {
  for (const uint8_t hard : {kX, kO}) {
    Game game;
    start(game);
    Tally tally;
    playHardAgainstEverything(game, hard, tally);
    CHECK(tally.losses == 0);
    CHECK(tally.wins > 0);
    CHECK(tally.draws > 0);
  }
}

// Perfect play from the start is a draw, so two HARD players always draw.
void testHardAgainstHardIsAlwaysADraw() {
  for (uint32_t seed = 1; seed <= 30; seed++) {
    Rng rng;
    rng.state = seed;
    Game game;
    start(game);
    while (!over(game)) CHECK(place(game, chooseCell(game, Level::Hard, rng)));
    CHECK(game.outcome == Outcome::Draw);
  }
}

// It takes the quickest win, not just any winning move.
void testHardWinsAtOnceWhenItCan() {
  // X X .
  // O O .
  // . . .   X to move: 2 wins now.
  const Game game = position("XX.OO....", kX);
  for (uint32_t seed = 1; seed <= 50; seed++) {
    Rng rng;
    rng.state = seed;
    CHECK(chooseCell(game, Level::Hard, rng) == 2);
  }
}

// Of the moves that are equally good it doesn't always choose the same one.
void testHardVariesItsOpening() {
  Game game;
  start(game);
  bool seen[kCells] = {};
  for (uint32_t seed = 1; seed <= 30; seed++) {
    Rng rng;
    rng.state = seed;
    seen[chooseCell(game, Level::Hard, rng)] = true;
  }
  int distinct = 0;
  for (const bool s : seen) distinct += s ? 1 : 0;
  CHECK(distinct > 1);
}

void testEasyAlwaysTakesAWin() {
  // X X .
  // O O .
  // . . .   O to move would win at 5; X to move wins at 2.
  const Game game = position("XX.OO....", kX);
  for (uint32_t seed = 1; seed <= 200; seed++) {
    Rng rng;
    rng.state = seed;
    CHECK(chooseCell(game, Level::Easy, rng) == 2);
  }
}

// Easy blocks sometimes, and sometimes doesn't.
void testEasyBlocksOnlySomeOfTheTime() {
  // X X .
  // O . .
  // . . .   O to move, with no win of its own: 2 blocks.
  const Game game = position("XX.O.....", kO);
  int blocks = 0;
  for (uint32_t seed = 1; seed <= 400; seed++) {
    Rng rng;
    rng.state = seed;
    const int cell = chooseCell(game, Level::Easy, rng);
    CHECK(canPlace(game, cell));
    if (cell == 2) blocks++;
  }
  CHECK(blocks > 100);
  CHECK(blocks < 400);
}

// Every move either level makes is legal, from every position real play reaches.
void playEveryPosition(const Game& game) {
  if (over(game)) {
    Rng rng;
    CHECK(chooseCell(game, Level::Easy, rng) == -1);
    CHECK(chooseCell(game, Level::Hard, rng) == -1);
    return;
  }
  Rng rng;
  rng.state = 2654435761u;
  CHECK(canPlace(game, chooseCell(game, Level::Easy, rng)));
  CHECK(canPlace(game, chooseCell(game, Level::Hard, rng)));
  for (int cell = 0; cell < kCells; cell++) {
    Game next = game;
    if (place(next, cell)) playEveryPosition(next);
  }
}

void testEveryChoiceIsLegal() {
  Game game;
  start(game);
  // Searching from every position of the whole tree is slow under the sanitizers;
  // one opening of each kind (corner, edge, centre) covers every shape by symmetry.
  for (const int opening : {0, 1, 4}) {
    Game next = game;
    CHECK(place(next, opening));
    playEveryPosition(next);
  }
}

// Hard against Easy: Hard never loses, and wins most of the time.
void testHardBeatsEasyMostOfTheTime() {
  int hardWins = 0;
  int easyWins = 0;
  constexpr int GAMES = 120;
  for (int i = 0; i < GAMES; i++) {
    Rng rng;
    rng.state = static_cast<uint32_t>(i + 1) * 2654435761u;
    Game game;
    start(game);
    const uint8_t hard = i % 2 == 0 ? kX : kO;
    while (!over(game)) CHECK(place(game, chooseCell(game, game.turn == hard ? Level::Hard : Level::Easy, rng)));
    if (game.outcome == Outcome::Draw) continue;
    if ((game.outcome == Outcome::XWins) == (hard == kX)) {
      hardWins++;
    } else {
      easyWins++;
    }
  }
  CHECK(easyWins == 0);
  CHECK(hardWins > GAMES / 2);
}

// A random mover can beat Easy: it isn't a wall.
void testEasyCanBeBeaten() {
  int randomWins = 0;
  for (int i = 0; i < 400; i++) {
    Rng rng;
    rng.state = static_cast<uint32_t>(i + 7) * 2246822519u;
    Game game;
    start(game);
    const uint8_t easy = i % 2 == 0 ? kX : kO;
    while (!over(game)) {
      int cell = 0;
      if (game.turn == easy) {
        cell = chooseCell(game, Level::Easy, rng);
      } else {
        do cell = rng.below(kCells);
        while (!canPlace(game, cell));
      }
      CHECK(place(game, cell));
    }
    if (game.outcome != Outcome::Draw && (game.outcome == Outcome::XWins) != (easy == kX)) randomWins++;
  }
  CHECK(randomWins > 0);
}

}  // namespace

int main() {
  testHardNeverLosesAsEitherMark();
  testHardAgainstHardIsAlwaysADraw();
  testHardWinsAtOnceWhenItCan();
  testHardVariesItsOpening();
  testEasyAlwaysTakesAWin();
  testEasyBlocksOnlySomeOfTheTime();
  testEveryChoiceIsLegal();
  testHardBeatsEasyMostOfTheTime();
  testEasyCanBeBeaten();

  std::printf("%d checks, %d failed\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
