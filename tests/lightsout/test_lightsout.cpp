// Lights Out's rules and saves, checked the way touch-licorice-apps' test_lightsout.py
// checks the Lua: puzzles are solvable with their target, presses, undo, hint, levels,
// and save and reload. The solver is also checked against every press set there is.
#include <cstdio>
#include <cstring>
#include <vector>

#include "lightsout_rules.h"
#include "lightsout_save.h"

using namespace lightsout;

namespace {

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    failures++;
  }
}

void testNewPuzzle() {
  uint32_t rng = 42;
  Game game;
  newPuzzle(game, kDefaultLevel, rng);
  check(game.board != 0 && game.moves == 0 && game.par >= 6, "a Normal puzzle is lit, untouched, with a target of at least 6");
  check(fewestPresses(game.board) == game.par, "the target is the fewest presses");
}

void testPressAndUndo() {
  uint32_t rng = 7;
  Game game;
  newPuzzle(game, kDefaultLevel, rng);
  const uint32_t before = game.board;
  check(press(game, 12), "a press is taken");
  check(game.board == (before ^ pressMask(12)) && game.moves == 1, "a press switches the square and its neighbours");
  check(pressMask(12) == ((1u << 12) | (1u << 7) | (1u << 17) | (1u << 11) | (1u << 13)), "the middle switches five");
  check(pressMask(0) == ((1u << 0) | (1u << 1) | (1u << 5)), "a corner switches three");
  check(undo(game) && game.board == before && game.moves == 0, "undo takes the press back");
  check(!undo(game), "nothing left to undo");
}

void testHintAndSolve() {
  uint32_t rng = 99;
  Game game;
  newPuzzle(game, kDefaultLevel, rng);
  const int par = game.par;
  const int first = hint(game);
  uint32_t x = 0;
  check(solve(game.board, x) && first >= 0 && ((x >> first) & 1), "the hint is part of a fewest-press solution");
  press(game, first);
  check(fewestPresses(game.board) == par - 1, "pressing the hint brings the target one closer");

  check(solve(game.board, x), "still solvable");
  for (int i = 0; i < kCells; i++) {
    if ((x >> i) & 1) press(game, i);
  }
  check(solved(game) && game.moves == par, "following the solver solves it in the target");
  check(!press(game, 3) && !undo(game) && hint(game) == -1, "a solved puzzle takes no presses, undo or hint");
}

void testLevels() {
  uint32_t rng = 1234;
  for (int level = 0; level < kLevelCount; level++) {
    for (int n = 0; n < 200; n++) {
      Game game;
      newPuzzle(game, level, rng);
      check(game.par >= kLevels[level].minimum, "the level's minimum target");
      check(game.par <= kLevels[level].presses, "no longer than the presses that made it");
      uint32_t x = 0;
      check(solve(game.board, x) && popcount(x) == game.par, "every puzzle has a solution of its target");
    }
  }
}

// The fewest presses for every board, by trying all 2^25 press sets in Gray code
// order, so each step adds or removes one press.
void testSolverAgainstEverything() {
  std::vector<uint8_t> fewest(1u << kCells, 0xFF);
  uint32_t board = 0;
  fewest[0] = 0;
  for (uint32_t n = 1; n < (1u << kCells); n++) {
    const uint32_t gray = n ^ (n >> 1);
    const int bit = __builtin_ctz(n);
    board ^= pressMask(bit);
    const uint8_t count = static_cast<uint8_t>(popcount(gray));
    if (count < fewest[board]) fewest[board] = count;
  }

  uint32_t rng = 555;
  int unsolvable = 0;
  for (int n = 0; n < 20000; n++) {
    const uint32_t b = nextRandom(rng) & kAllLights;
    const int expected = fewest[b] == 0xFF ? -1 : fewest[b];
    if (expected < 0) unsolvable++;
    if (fewestPresses(b) != expected) {
      std::printf("board %08x: solver %d, every press set %d\n", b, fewestPresses(b), expected);
      check(false, "the solver finds the fewest presses");
      break;
    }
  }
  check(unsolvable > 0, "some boards can't be solved, and the solver says so");
}

void testHistoryCap() {
  uint32_t rng = 3;
  Game game;
  newPuzzle(game, kDefaultLevel, rng);
  // Pressing a square twice changes nothing, so this never solves it.
  for (int n = 0; n < 300; n++) press(game, 0);
  check(game.historyCount == kHistory && game.moves == 300, "history keeps the last 255 presses");
  int undone = 0;
  while (undo(game)) undone++;
  check(undone == kHistory && game.moves == 300 - kHistory, "undo reaches back 255 presses");
}

void testSave() {
  uint32_t rng = 77;
  Game game;
  newPuzzle(game, 2, rng);
  press(game, 0);
  press(game, 24);

  uint8_t bytes[SAVE_MAX];
  const size_t size = packGame(game, bytes);
  check(size == SAVE_HEADER + 2, "a save holds its history");

  Game back;
  check(unpackGame(SAVE_VERSION, bytes, size, back), "round trip loads");
  check(back.board == game.board && back.moves == 2 && back.par == game.par && back.historyCount == 2 &&
            back.history[0] == 0 && back.history[1] == 24,
        "round trip matches");
  const uint32_t pressedBoard = back.board;
  check(undo(back) && back.board == (pressedBoard ^ pressMask(24)) && back.moves == 1, "undo works after a reload");

  Game untouched;
  newPuzzle(untouched, 0, rng);
  const uint32_t keep = untouched.board;
  uint8_t bad[SAVE_MAX];
  auto rejects = [&](const char* what, auto change, size_t badSize) {
    std::memcpy(bad, bytes, size);
    change();
    check(!unpackGame(SAVE_VERSION, bad, badSize, untouched), what);
  };
  check(!unpackGame(SAVE_VERSION + 1, bytes, size, untouched), "rejects another version");
  rejects("rejects a short save", [] {}, size - 1);
  rejects("rejects a solved board", [&] { std::memset(bad, 0, 4); }, size);
  rejects("rejects a light past the board", [&] { bad[3] |= 0x02; }, size);
  rejects("rejects an unsolvable board", [&] {
    uint32_t b = 0;
    uint32_t r = 9;
    do b = nextRandom(r) & kAllLights; while (fewestPresses(b) >= 0);
    for (int i = 0; i < 4; i++) bad[i] = static_cast<uint8_t>(b >> (8 * i));
  }, size);
  rejects("rejects a target of 0", [&] { bad[6] = 0; }, size);
  rejects("rejects a square past the board", [&] { bad[SAVE_HEADER] = kCells; }, size);
  rejects("rejects fewer moves than history", [&] { bad[4] = 1; bad[5] = 0; }, size);
  rejects("rejects a count that doesn't match the size", [&] { bad[7] = 3; }, size);
  check(untouched.board == keep, "a rejected save leaves the game alone");

  Stats stats{12345, 2};
  uint8_t sb[STATS_BYTES];
  check(packStats(stats, sb) == STATS_BYTES, "stats size");
  Stats sback;
  check(unpackStats(STATS_VERSION, sb, STATS_BYTES, sback) && sback.solved == 12345 && sback.level == 2, "stats round trip");
  sb[4] = kLevelCount;
  check(!unpackStats(STATS_VERSION, sb, STATS_BYTES, sback) && sback.level == 2, "rejects a level past the list");
}

}  // namespace

int main() {
  testNewPuzzle();
  testPressAndUndo();
  testHintAndSolve();
  testLevels();
  testSolverAgainstEverything();
  testHistoryCap();
  testSave();
  if (failures) {
    std::printf("%d failure(s)\n", failures);
    return 1;
  }
  std::printf("lightsout: all passed\n");
  return 0;
}
