// Dice Roller with no panel: faces stay 1 to 6 and none is favoured, the rejection step throws
// back the values that would favour some, both dice and the totals come out as often as they
// should even from the weakest entropy, and the save.
#include <cstdio>
#include <vector>

#include "diceroller_rules.h"
#include "diceroller_save.h"

using namespace diceroller;

namespace {

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    failures++;
  }
}

// Pearson's chi-squared statistic of counts against their expected counts.
double chiSquared(const std::vector<long>& counts, const std::vector<double>& expected) {
  double sum = 0;
  for (size_t i = 0; i < counts.size(); i++) {
    const double d = counts[i] - expected[i];
    sum += d * d / expected[i];
  }
  return sum;
}

void testRejection() {
  check(kLimit == 4294967291u && (static_cast<uint64_t>(kLimit) + 1) % kFaces == 0, "the limit keeps a whole number of each face");
  const uint32_t values[] = {0xFFFFFFFFu, 0xFFFFFFFCu, kLimit};
  int at = 0;
  const uint8_t f = face([&] { return values[at++]; });
  check(at == 3 && f == kLimit % kFaces + 1, "values past the limit are thrown back, and the limit itself kept");
  at = 0;
  const uint32_t low[] = {0, 5, 6};
  check(face([&] { return low[at++]; }) == 1 && face([&] { return low[at++]; }) == 6 && face([&] { return low[at++]; }) == 1,
        "0 to 5 read as 1 to 6");
}

void testFair() {
  // The weakest entropy there could be: 0, 1, 2 and so on, as a tap timed in whole
  // microseconds with the chip's number stuck. The mixing must still give fair dice.
  constexpr long kRolls = 600000;
  Source source;
  std::vector<long> faces(kFaces, 0);
  std::vector<long> pairs(kFaces * kFaces, 0);
  std::vector<long> totals(13, 0);
  std::vector<long> repeats(2, 0);
  int previous = 0;
  bool inRange = true;
  for (long i = 0; i < kRolls; i++) {
    Roll r;
    roll(r, source, static_cast<uint64_t>(i));
    for (int d = 0; d < kDice; d++) {
      inRange = inRange && r.dice[d] >= 1 && r.dice[d] <= kFaces;
      faces[r.dice[d] - 1]++;
    }
    pairs[(r.dice[0] - 1) * kFaces + (r.dice[1] - 1)]++;
    totals[total(r)]++;
    repeats[total(r) == previous]++;
    previous = total(r);
  }
  check(inRange, "every die is 1 to 6");
  // Thresholds at p = 0.001, so a fair roller fails one in a thousand runs, and the seed is fixed.
  check(chiSquared(faces, std::vector<double>(kFaces, 2.0 * kRolls / kFaces)) < 20.52, "each face as often as the others");
  check(chiSquared(pairs, std::vector<double>(kFaces * kFaces, 1.0 * kRolls / 36)) < 66.62,
        "all 36 pairs as often, so the two dice are independent");
  std::vector<long> t(totals.begin() + 2, totals.end());
  std::vector<double> e;
  for (int s = 2; s <= 12; s++) e.push_back(kRolls * (6 - (s > 7 ? s - 7 : 7 - s)) / 36.0);
  check(chiSquared(t, e) < 29.59, "totals 2 to 12 on the 1-2-3-4-5-6-5-4-3-2-1 curve");
  // The chance a total repeats the last one is the sum of each total's chance squared: 146/1296.
  const double expectRepeat = kRolls * 146.0 / 1296.0;
  check(chiSquared(repeats, {kRolls - expectRepeat, expectRepeat}) < 10.83, "one roll says nothing about the next");
}

void testState() {
  Source a;
  Source b;
  a.stir(1);
  Roll ra;
  Roll rb;
  roll(ra, a, 42);
  roll(rb, b, 42);
  bool differ = false;
  for (int k = 0; k < 20 && !differ; k++) {
    roll(ra, a, 42);
    roll(rb, b, 42);
    differ = ra.dice[0] != rb.dice[0] || ra.dice[1] != rb.dice[1];
  }
  check(differ, "the same entropy gives different dice once the history differs");
  Roll fresh;
  check(!rolled(fresh) && total(fresh) == 0, "nothing rolled yet");
}

void testCount() {
  Roll r;
  Source source;
  check(r.rolls == 0, "no rolls yet");
  for (int i = 0; i < 5; i++) roll(r, source, i);
  check(r.rolls == 5 && rolled(r), "each roll adds one");
  reset(r);
  check(r.rolls == 0 && !rolled(r) && total(r) == 0, "reset clears the count and the dice");
  roll(r, source, 9);
  check(r.rolls == 1, "counting starts again after a reset");
  r.rolls = kMaxRolls;
  roll(r, source, 10);
  check(r.rolls == kMaxRolls && rolled(r), "the count stops at its limit, and the dice still roll");
}

void testSave() {
  Roll r;
  r.dice[0] = 3;
  r.dice[1] = 6;
  r.rolls = 70000;  // over 16 bits, so every count byte is checked
  uint8_t data[SAVE_BYTES];
  const size_t size = pack(r, data);
  Roll back;
  check(unpack(SAVE_VERSION, data, size, back) && back.dice[0] == 3 && back.dice[1] == 6 && total(back) == 9, "the roll round-trips");
  check(back.rolls == 70000, "so does the count");
  const uint8_t version1[2] = {3, 6};
  check(!unpack(1, version1, sizeof(version1), back), "a version 1 save, without a count, is refused");
  Roll zero = r;
  zero.rolls = 0;
  uint8_t zeroData[SAVE_BYTES];
  pack(zero, zeroData);
  check(!unpack(SAVE_VERSION, zeroData, size, back), "dice with a count of 0 are refused");
  Roll over = r;
  over.rolls = kMaxRolls + 1;
  uint8_t overData[SAVE_BYTES];
  pack(over, overData);
  check(!unpack(SAVE_VERSION, overData, size, back), "a count past the limit is refused");
  check(!unpack(SAVE_VERSION + 1, data, size, back), "another version is refused");
  check(!unpack(SAVE_VERSION, data, size - 1, back), "a short one");
  data[1] = 7;
  Roll keep;
  keep.dice[0] = 2;
  keep.dice[1] = 2;
  check(!unpack(SAVE_VERSION, data, size, keep) && keep.dice[0] == 2, "a 7 is refused, leaving the roll alone");
  data[1] = 0;
  check(!unpack(SAVE_VERSION, data, size, keep), "so is a 0");
}

}  // namespace

int main() {
  testRejection();
  testFair();
  testState();
  testCount();
  testSave();
  if (failures) {
    std::printf("%d failure(s)\n", failures);
    return 1;
  }
  std::printf("diceroller: all passed\n");
  return 0;
}
