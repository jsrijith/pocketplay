// Tic Tac Toe's save: version 2 round trips with its mode, version 1 still loads,
// and bytes no real game could leave are refused.

#include <cstdio>
#include <cstring>

#include "tictactoe_save.h"

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

Saved sample(Mode mode, uint8_t computer) {
  Saved saved;
  start(saved.game);
  place(saved.game, 4);  // X in the centre
  place(saved.game, 0);  // O in a corner
  place(saved.game, 8);  // X opposite: O to move
  saved.mode = mode;
  saved.computer = computer;
  return saved;
}

bool sameGame(const Saved& a, const Saved& b) {
  return std::memcmp(a.game.cell, b.game.cell, kCells) == 0 && a.game.turn == b.game.turn &&
         a.game.outcome == b.game.outcome && a.mode == b.mode && a.computer == b.computer;
}

void testEachModeRoundTrips() {
  const Saved cases[] = {sample(Mode::TwoPlayers, kEmpty), sample(Mode::Easy, kO), sample(Mode::Hard, kX)};
  for (const Saved& saved : cases) {
    uint8_t bytes[SAVE_BYTES];
    CHECK(packGame(saved, bytes) == SAVE_BYTES);
    Saved loaded = sample(Mode::Easy, kX);
    CHECK(unpackGame(SAVE_VERSION, bytes, SAVE_BYTES, loaded));
    CHECK(sameGame(saved, loaded));
  }
}

void testVersionOneLoadsAsTwoPlayers() {
  // The board and turn alone, as version 1 wrote them.
  const uint8_t bytes[SAVE_BYTES_V1] = {kO, 0, 0, 0, kX, 0, 0, 0, kX, kO};
  Saved loaded = sample(Mode::Hard, kX);
  CHECK(unpackGame(1, bytes, sizeof(bytes), loaded));
  CHECK(loaded.mode == Mode::TwoPlayers);
  CHECK(loaded.computer == kEmpty);
  CHECK(sameGame(loaded, sample(Mode::TwoPlayers, kEmpty)));
}

void testRefusesAModeThatDoesNotFit() {
  const Saved before = sample(Mode::Easy, kO);
  uint8_t good[SAVE_BYTES];
  packGame(before, good);

  struct Bad {
    uint8_t mode;
    uint8_t computer;
  };
  const Bad bad[] = {
      {kModeCount, kO},                                  // no such mode
      {static_cast<uint8_t>(Mode::TwoPlayers), kO},      // two players, but a computer
      {static_cast<uint8_t>(Mode::Easy), kEmpty},        // the computer has no mark
      {static_cast<uint8_t>(Mode::Hard), 3},             // not a mark
  };
  for (const Bad& b : bad) {
    uint8_t bytes[SAVE_BYTES];
    std::memcpy(bytes, good, sizeof(bytes));
    bytes[kCells + 1] = b.mode;
    bytes[kCells + 2] = b.computer;
    Saved loaded = before;
    CHECK(!unpackGame(SAVE_VERSION, bytes, sizeof(bytes), loaded));
    CHECK(sameGame(loaded, before));
  }
}

void testRefusesWrongSizesAndVersions() {
  uint8_t bytes[SAVE_BYTES];
  packGame(sample(Mode::Hard, kX), bytes);
  Saved loaded = sample(Mode::TwoPlayers, kEmpty);
  CHECK(!unpackGame(SAVE_VERSION, bytes, SAVE_BYTES - 1, loaded));
  CHECK(!unpackGame(1, bytes, SAVE_BYTES, loaded));
  CHECK(!unpackGame(3, bytes, SAVE_BYTES, loaded));
}

void testRefusesABoardRealPlayCannotReach() {
  Saved saved = sample(Mode::Easy, kO);
  uint8_t bytes[SAVE_BYTES];
  packGame(saved, bytes);
  bytes[kCells] = kX;  // O has to move after three marks
  Saved loaded = sample(Mode::TwoPlayers, kEmpty);
  CHECK(!unpackGame(SAVE_VERSION, bytes, sizeof(bytes), loaded));

  // A finished game is never saved.
  const uint8_t won[SAVE_BYTES] = {kX, kX, kX, kO, kO, 0, 0, 0, 0, kO, static_cast<uint8_t>(Mode::Easy), kO};
  CHECK(!unpackGame(SAVE_VERSION, won, sizeof(won), loaded));
}

}  // namespace

int main() {
  testEachModeRoundTrips();
  testVersionOneLoadsAsTwoPlayers();
  testRefusesAModeThatDoesNotFit();
  testRefusesWrongSizesAndVersions();
  testRefusesABoardRealPlayCannotReach();

  std::printf("%d checks, %d failed\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
