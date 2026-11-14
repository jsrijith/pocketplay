// Crown & Crest with no panel: faces stay 1 to 6 and none is favoured on any of the four dice,
// even from the weakest entropy, the rejection step throws back the values that would favour
// some, the face art is all there, the draft, and the save.
#include <cstdio>
#include <vector>

#include "crowncrest_faces.h"
#include "crowncrest_rules.h"
#include "crowncrest_save.h"

using namespace crowncrest;

namespace {

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    failures++;
  }
}

double chiSquared(const std::vector<long>& counts, double expected) {
  double sum = 0;
  for (long c : counts) {
    const double d = c - expected;
    sum += d * d / expected;
  }
  return sum;
}

void testRejection() {
  check(kLimit == 4294967291u && (static_cast<uint64_t>(kLimit) + 1) % kFaces == 0, "the limit keeps a whole number of each face");
  const uint32_t values[] = {0xFFFFFFFFu, 0xFFFFFFFCu, kLimit};
  int at = 0;
  const uint8_t f = face([&] { return values[at++]; });
  check(at == 3 && f == kLimit % kFaces + 1, "values past the limit are thrown back, and the limit itself kept");
}

void testFair() {
  // The weakest entropy there could be: 0, 1, 2 and so on, as a tap timed in whole
  // microseconds with the chip's number stuck. The mixing must still give fair dice.
  constexpr long kRolls = 600000;
  Source source;
  std::vector<std::vector<long>> faces(kDice, std::vector<long>(kFaces, 0));
  std::vector<long> pairs(kFaces * kFaces, 0);
  bool inRange = true;
  for (long i = 0; i < kRolls; i++) {
    Roll r;
    roll(r, source, static_cast<uint64_t>(i));
    for (int d = 0; d < kDice; d++) {
      inRange = inRange && r.face[d] >= 1 && r.face[d] <= kFaces;
      if (r.face[d] >= 1 && r.face[d] <= kFaces) faces[d][r.face[d] - 1]++;
    }
    if (r.face[0] >= 1 && r.face[0] <= kFaces && r.face[3] >= 1 && r.face[3] <= kFaces) {
      pairs[(r.face[0] - 1) * kFaces + r.face[3] - 1]++;
    }
  }
  check(inRange, "every face is 1 to 6");
  // 99.9th percentiles: 20.5 for 5 degrees of freedom, 66.6 for 35.
  for (int d = 0; d < kDice; d++) {
    check(chiSquared(faces[d], kRolls / 6.0) < 20.5, "each die's faces come up evenly");
  }
  check(chiSquared(pairs, kRolls / 36.0) < 66.6, "the first and last dice don't follow each other");
}

void testFaces() {
  using namespace crowncrestFaces;
  check(FACE == 180 && ROW_BYTES == 23, "faces are 180 x 180");
  check(sizeof(FACES) == static_cast<size_t>(kDice * kFaces * ROW_BYTES * FACE), "24 faces");
  for (int d = 0; d < kDice; d++) {
    for (int f = 0; f < kFaces; f++) {
      long ink = 0;
      for (int i = 0; i < ROW_BYTES * FACE; i++) {
        for (int b = 0; b < 8; b++) ink += (FACES[d][f][i] & (0x80 >> b)) == 0;
      }
      // Padding bits are white, so ink is only the art: some, but far from a solid square.
      check(ink > 500 && ink < FACE * FACE / 2, "every face has art on it");
    }
  }
}

void testDraft() {
  Turn t;
  Source source;
  check(nextRoller(t) == 0 && canRoll(t) && picker(t) == kNoOne, "A rolls first, and nothing is picked before a roll");
  check(!pick(t, 0) && !undoPick(t), "no picks before the first roll");
  for (int d = 0; d < kDice; d++) check(owner(t, d) == kNoOne, "no die is owned before a roll");

  check(rollTurn(t, source, 1) && t.roller == 0 && rolled(t.dice), "A rolls");
  check(!canRoll(t) && !rollTurn(t, source, 2), "no roll while dice are unpicked");
  check(picker(t) == 0, "the roller picks first");
  check(pick(t, 2) && owner(t, 2) == 0, "A takes one die");
  check(!pick(t, 2), "a taken die can't be taken again");
  check(picker(t) == 1, "then B picks");
  check(pick(t, 0) && owner(t, 0) == 1 && picker(t) == 1, "B takes one and picks again");
  check(undoPick(t) && owner(t, 0) == kNoOne && picker(t) == 1, "undo takes back B's pick");
  check(pick(t, 3) && pick(t, 0), "B takes two");
  check(done(t) && picker(t) == kNoOne, "after three picks the turn is shared out");
  check(owner(t, 1) == 0 && owner(t, 2) == 0 && owner(t, 0) == 1 && owner(t, 3) == 1,
        "A has its pick and the last die, B has two");
  check(!pick(t, 1), "the last die isn't picked");

  check(canRoll(t) && nextRoller(t) == 1, "B rolls next");
  check(rollTurn(t, source, 3) && t.roller == 1 && t.picks == 0 && picker(t) == 1, "B rolls and picks first");
  for (int d = 0; d < kDice; d++) check(owner(t, d) == kNoOne, "a new roll frees every die");
  pick(t, 1);
  check(picker(t) == 0 && pick(t, 2) && pick(t, 3), "then A takes two");
  check(owner(t, 1) == 1 && owner(t, 0) == 1 && owner(t, 2) == 0 && owner(t, 3) == 0, "B has its pick and the last die");
  check(undoPick(t) && !done(t) && owner(t, 0) == kNoOne, "undo reopens the last pick");
  pick(t, 3);
  check(rollTurn(t, source, 4) && t.roller == 0, "A rolls again after B");
}

void testSave() {
  Turn t;
  Source source;
  uint8_t data[SAVE_BYTES];
  Turn back;
  check(pack(t, data) == SAVE_BYTES && unpack(SAVE_VERSION, data, SAVE_BYTES, back) && !rolled(back.dice),
        "a blank turn comes back blank");
  rollTurn(t, source, 42);
  pick(t, 1);
  pick(t, 3);
  pack(t, data);
  check(unpack(SAVE_VERSION, data, SAVE_BYTES, back), "a save comes back");
  bool same = back.roller == t.roller && back.picks == 2 && back.order[0] == 1 && back.order[1] == 3;
  for (int d = 0; d < kDice; d++) same = same && back.dice.face[d] == t.dice.face[d];
  check(same, "the round trip keeps the dice, the roller and the picks");

  Turn untouched;
  check(!unpack(SAVE_VERSION - 1, data, SAVE_BYTES, untouched), "an old version is refused");
  check(!unpack(SAVE_VERSION, data, SAVE_BYTES - 1, untouched), "a short save is refused");
  const uint8_t blankDie[SAVE_BYTES] = {1, 2, 0, 4, 0, 0, 0, 0, 0};
  check(!unpack(SAVE_VERSION, blankDie, SAVE_BYTES, untouched), "one blank die is refused");
  const uint8_t seven[SAVE_BYTES] = {1, 7, 3, 4, 0, 0, 0, 0, 0};
  check(!unpack(SAVE_VERSION, seven, SAVE_BYTES, untouched), "a face past 6 is refused");
  const uint8_t roller[SAVE_BYTES] = {1, 2, 3, 4, 2, 0, 0, 0, 0};
  check(!unpack(SAVE_VERSION, roller, SAVE_BYTES, untouched), "a third player is refused");
  const uint8_t picks[SAVE_BYTES] = {1, 2, 3, 4, 0, 4, 0, 1, 2};
  check(!unpack(SAVE_VERSION, picks, SAVE_BYTES, untouched), "four picks are refused");
  const uint8_t twice[SAVE_BYTES] = {1, 2, 3, 4, 0, 2, 1, 1, 0};
  check(!unpack(SAVE_VERSION, twice, SAVE_BYTES, untouched), "the same die picked twice is refused");
  const uint8_t far[SAVE_BYTES] = {1, 2, 3, 4, 0, 1, 4, 0, 0};
  check(!unpack(SAVE_VERSION, far, SAVE_BYTES, untouched), "a fifth die is refused");
  const uint8_t blankPicked[SAVE_BYTES] = {0, 0, 0, 0, 0, 1, 0, 0, 0};
  check(!unpack(SAVE_VERSION, blankPicked, SAVE_BYTES, untouched), "picks before a roll are refused");
  check(!rolled(untouched.dice), "a refused save leaves the turn alone");
}

}  // namespace

int main() {
  testRejection();
  testFair();
  testFaces();
  testDraft();
  testSave();
  if (failures) {
    std::printf("crowncrest: %d failed\n", failures);
    return 1;
  }
  std::printf("crowncrest: all passed\n");
  return 0;
}
