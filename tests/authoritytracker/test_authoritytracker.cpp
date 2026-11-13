// Authority Tracker with no panel: scores start at 50 and stop at 0 and 999, a score of 0
// ends the game and freezes every button but the loser's +, and the save round-trips and
// rejects bad bytes.
#include <cstdio>

#include "authoritytracker_rules.h"
#include "authoritytracker_save.h"

using namespace authoritytracker;

namespace {

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    failures++;
  }
}

void testStart() {
  State s;
  check(s.score[0] == 50 && s.score[1] == 50, "both start at 50");
  check(loser(s) == -1 && atStart(s), "a new game is on, at the start");
}

void testMoves() {
  State s;
  check(apply(s, 0, 10) && s.score[0] == 60, "+10");
  check(apply(s, 1, -5) && s.score[1] == 45, "-5");
  check(!atStart(s), "moved off the start");
  check(!apply(s, 2, 1) && !apply(s, -1, 1) && !apply(s, 0, 0), "no such player or amount");
  s.score[0] = 995;
  check(apply(s, 0, 10) && s.score[0] == kMax, "stops at 999");
  check(!canApply(s, 0, 1) && !apply(s, 0, 1), "+ at 999 does nothing");
  check(canApply(s, 0, -1), "- at 999 still works");
}

void testEnd() {
  State s;
  s.score[1] = 3;
  check(apply(s, 1, -10) && s.score[1] == 0, "stops at 0");
  check(loser(s) == 1, "0 ends the game");
  check(!canApply(s, 1, -1), "- for the loser does nothing");
  check(!canApply(s, 0, 1) && !canApply(s, 0, -1), "the winner's buttons stop");
  check(apply(s, 1, 5) && s.score[1] == 5 && loser(s) == -1, "the loser's + takes it back, and the game goes on");
  reset(s);
  check(atStart(s), "reset goes back to 50 and 50");
}

void testSave() {
  State s;
  s.score[0] = 999;
  s.score[1] = 0;
  uint8_t data[SAVE_BYTES];
  check(pack(s, data) == SAVE_BYTES, "packs to its size");
  State back;
  check(unpack(SAVE_VERSION, data, SAVE_BYTES, back) && back.score[0] == 999 && back.score[1] == 0, "round trip");

  State untouched;
  check(!unpack(SAVE_VERSION + 1, data, SAVE_BYTES, untouched), "wrong version rejected");
  check(!unpack(SAVE_VERSION, data, SAVE_BYTES - 1, untouched), "wrong size rejected");
  const uint8_t tooBig[SAVE_BYTES] = {0x03, 0xE8, 0, 50};  // 1000
  check(!unpack(SAVE_VERSION, tooBig, SAVE_BYTES, untouched), "a score over 999 rejected");
  check(atStart(untouched), "a rejected save leaves the state alone");
}

}  // namespace

int main() {
  testStart();
  testMoves();
  testEnd();
  testSave();
  if (failures) {
    std::printf("%d failure(s)\n", failures);
    return 1;
  }
  std::printf("authoritytracker: all passed\n");
  return 0;
}
