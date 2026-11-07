// Symbol Dice's rolls, turns and save, checked on the PC.
#include <cstdio>

#include "symboldice_rules.h"
#include "symboldice_save.h"

using namespace symboldice;

namespace {

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    failures++;
  }
}

// A small xorshift, standing in for the chip's random numbers.
uint32_t next(uint32_t& s) {
  s ^= s << 13;
  s ^= s >> 17;
  s ^= s << 5;
  return s;
}

void testRolls() {
  Game g;
  check(!started(g) && !over(g), "a new game hasn't started");
  check(roll(g, 6 * 4 + 2) && g.turn == 1 && g.dice[0] == 2 && g.dice[1] == 4, "a roll sets both dice and takes a turn");
  for (int t = 2; t <= kTurns; t++) check(roll(g, static_cast<uint32_t>(t)) && g.turn == t, "each roll is a turn");
  check(over(g), "after 12 turns the game is over");
  const Game before = g;
  check(!roll(g, 99) && g.turn == kTurns && g.dice[0] == before.dice[0] && g.dice[1] == before.dice[1], "a 13th roll changes nothing");
  reset(g);
  check(g.turn == 0 && g.dice[0] == 0 && g.dice[1] == 0 && !started(g), "reset starts again");

  // Every pair comes up, and evenly enough.
  int seen[kSymbols][kSymbols] = {};
  uint32_t s = 2463534242u;
  for (int k = 0; k < 36000; k++) {
    Game r;
    roll(r, next(s));
    seen[r.dice[0]][r.dice[1]]++;
  }
  bool all = true;
  bool even = true;
  for (auto& row : seen) {
    for (int count : row) {
      all = all && count > 0;
      even = even && count > 800 && count < 1200;
    }
  }
  check(all, "all 36 pairs come up");
  check(even, "and about equally often");
}

void testSave() {
  Game g;
  roll(g, 6 * 5 + 1);
  roll(g, 3);
  uint8_t data[SAVE_BYTES];
  check(pack(g, data) == SAVE_BYTES, "a save is three bytes");
  Game back;
  check(unpack(SAVE_VERSION, data, SAVE_BYTES, back) && back.turn == 2 && back.dice[0] == 3 && back.dice[1] == 0, "and loads");

  auto refused = [](uint8_t version, const uint8_t* bytes, size_t size, const char* what) {
    Game t;
    roll(t, 7);
    check(!unpack(version, bytes, size, t) && t.turn == 1 && t.dice[0] == 1 && t.dice[1] == 1, what);
  };
  refused(SAVE_VERSION + 1, data, SAVE_BYTES, "another version is refused");
  refused(SAVE_VERSION, data, SAVE_BYTES - 1, "a short save is refused");
  const uint8_t zero[] = {0, 0, 0};
  refused(SAVE_VERSION, zero, SAVE_BYTES, "turn 0 is refused");
  const uint8_t late[] = {kTurns + 1, 0, 0};
  refused(SAVE_VERSION, late, SAVE_BYTES, "a turn past 12 is refused");
  const uint8_t die[] = {3, kSymbols, 0};
  refused(SAVE_VERSION, die, SAVE_BYTES, "a die that isn't a symbol is refused");
  const uint8_t last[] = {kTurns, 5, 5};
  Game done;
  check(unpack(SAVE_VERSION, last, SAVE_BYTES, done) && over(done), "a finished game loads finished");
}

}  // namespace

int main() {
  testRolls();
  testSave();
  if (failures) {
    std::printf("%d failure(s)\n", failures);
    return 1;
  }
  std::printf("symboldice: all passed\n");
  return 0;
}
