// Bingo Caller with no panel: every mode calls each number exactly once and then stops, no number
// is favoured even from the weakest entropy, the rejection step throws back the values that would
// favour some, 75-ball's letters, and the save.
#include <cstdio>
#include <cstring>
#include <vector>

#include "bingocaller_rules.h"
#include "bingocaller_save.h"

using namespace bingocaller;

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

void testWholeGames() {
  for (int mode = 0; mode < kModes; mode++) {
    State s;
    start(s, mode);
    Source source;
    check(s.count == 0 && last(s) == 0 && !finished(s), "a new game has nothing called");
    for (int i = 0; i < balls(s); i++) {
      check(call(s, source, static_cast<uint64_t>(i)), "a call succeeds while numbers are left");
      check(last(s) >= 1 && last(s) <= balls(s), "a call is in range");
    }
    check(finished(s), "every number called finishes the game");
    bool seen[kMaxBalls + 1] = {};
    bool repeat = false;
    for (int i = 0; i < s.count; i++) {
      repeat = repeat || seen[s.order[i]];
      seen[s.order[i]] = true;
    }
    check(!repeat, "no number is called twice");
    for (int n = 1; n <= balls(s); n++) check(isCalled(s, n), "every number is called once");

    const State before = s;
    check(!call(s, source, 99), "no call after the last number");
    check(std::memcmp(&before, &s, sizeof(State)) == 0, "a refused call changes nothing");
  }
}

// The first call of a 30-ball game, from entropy that is only a counter: each number should come
// up 1 time in 30. Chi-squared with 29 degrees of freedom is under 58.3 at p = 0.001.
void testFirstCallFair() {
  constexpr int kTrials = 300000;
  std::vector<long> counts(30, 0);
  Source source;
  for (int t = 0; t < kTrials; t++) {
    State s;
    start(s, 0);
    call(s, source, static_cast<uint64_t>(t));
    counts[last(s) - 1]++;
  }
  check(chiSquared(counts, kTrials / 30.0) < 58.3, "the first call favours no number");
}

// Where number 1 lands in a whole 90-ball game: every position equally often. Chi-squared with
// 89 degrees of freedom is under 135 at p = 0.001.
void testPositionFair() {
  constexpr int kTrials = 90000;
  std::vector<long> counts(90, 0);
  Source source;
  for (int t = 0; t < kTrials; t++) {
    State s;
    start(s, 3);
    while (call(s, source, static_cast<uint64_t>(t))) {
    }
    for (int i = 0; i < 90; i++) {
      if (s.order[i] == 1) counts[i]++;
    }
  }
  check(chiSquared(counts, kTrials / 90.0) < 135, "number 1 lands in every position equally");
}

void testRejection() {
  // 2^32 isn't a multiple of 90, so the top values must be thrown back.
  const uint32_t values[] = {0xFFFFFFFFu, 0xFFFFFFFEu, 7};
  int k = 0;
  const uint32_t r = uniform(90, [&] { return values[k++]; });
  check(k == 3 && r == 7, "values that would favour some numbers are thrown back");
  int calls = 0;
  check(uniform(1, [&] { calls++; return 0xFFFFFFFFu; }) == 0 && calls == 1, "one choice takes the first value");
}

void testLetters() {
  State s;
  start(s, 1);
  check(letter(s, 1) == 'B' && letter(s, 15) == 'B', "1 to 15 are B");
  check(letter(s, 16) == 'I' && letter(s, 31) == 'N' && letter(s, 60) == 'G' && letter(s, 75) == 'O', "the columns follow BINGO");
  start(s, 3);
  check(letter(s, 5) == '\0', "90-ball has no letters");
}

void testSave() {
  State s;
  start(s, 2);
  Source source;
  for (int i = 0; i < 20; i++) call(s, source, static_cast<uint64_t>(i) * 31);
  uint8_t bytes[MAX_SAVE_BYTES];
  const size_t size = pack(s, bytes);
  check(size == SAVE_HEADER + 20, "the save is the header and one byte a call");

  State loaded;
  check(unpack(SAVE_VERSION, bytes, size, loaded), "a save loads");
  check(std::memcmp(&s, &loaded, sizeof(State)) == 0, "a save loads the same calls in order");

  State full;
  start(full, 3);
  while (call(full, source, 5)) {
  }
  check(pack(full, bytes) == MAX_SAVE_BYTES, "a finished 90-ball game fits the largest save");
  check(unpack(SAVE_VERSION, bytes, MAX_SAVE_BYTES, loaded) && finished(loaded), "a finished game loads");

  State untouched;
  start(untouched, 1);
  auto rejects = [&](const uint8_t* in, size_t n, uint8_t version, const char* what) {
    State keep = untouched;
    check(!unpack(version, in, n, keep), what);
    check(std::memcmp(&keep, &untouched, sizeof(State)) == 0, "a refused save changes nothing");
  };
  const uint8_t good[] = {0, 3, 5, 9, 30};
  check(unpack(SAVE_VERSION, good, sizeof(good), loaded), "a small save loads");
  rejects(good, sizeof(good), SAVE_VERSION + 1, "another version is refused");
  rejects(good, sizeof(good) - 1, SAVE_VERSION, "a short save is refused");
  rejects(good, 1, SAVE_VERSION, "a save without its header is refused");
  const uint8_t badMode[] = {4, 0};
  rejects(badMode, sizeof(badMode), SAVE_VERSION, "an unknown mode is refused");
  const uint8_t tooMany[] = {0, 31};
  rejects(tooMany, sizeof(tooMany), SAVE_VERSION, "more calls than balls is refused");
  const uint8_t outOfRange[] = {0, 2, 5, 31};
  rejects(outOfRange, sizeof(outOfRange), SAVE_VERSION, "a number past the last ball is refused");
  const uint8_t zero[] = {0, 1, 0};
  rejects(zero, sizeof(zero), SAVE_VERSION, "a zero is refused");
  const uint8_t repeat[] = {1, 3, 5, 9, 5};
  rejects(repeat, sizeof(repeat), SAVE_VERSION, "a repeated number is refused");
}

}  // namespace

int main() {
  testWholeGames();
  testFirstCallFair();
  testPositionFair();
  testRejection();
  testLetters();
  testSave();
  if (failures != 0) {
    std::printf("bingocaller: %d failed\n", failures);
    return 1;
  }
  std::printf("bingocaller: all passed\n");
  return 0;
}
