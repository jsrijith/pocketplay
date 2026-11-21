// Karuba Tiles with no panel: the tile table matches the printed set, a round draws each tile
// exactly once and then stops, no tile or position is favoured even from the weakest entropy, the
// rejection step throws back the values that would favour some, and the save.
#include <cstdio>
#include <cstring>
#include <vector>

#include "karubatiles_rules.h"
#include "karubatiles_save.h"

using namespace karubatiles;

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

int edges(uint8_t paths) {
  return ((paths & kNorth) != 0) + ((paths & kEast) != 0) + ((paths & kSouth) != 0) + ((paths & kWest) != 0);
}

// The printed set: 10 crystals and 4 gold nuggets, every tile a path of at least two ends, and
// how many of each shape there are.
void testTileSet() {
  int crystals = 0, gold = 0, straights = 0, bends = 0, tees = 0, crossings = 0;
  for (int n = 1; n <= kTiles; n++) {
    const Tile& t = tile(n);
    check((t.paths & ~(kNorth | kEast | kSouth | kWest)) == 0, "a tile's paths name only its four edges");
    check(edges(t.paths) >= 2, "every tile's path has at least two ends");
    crystals += t.treasure == Treasure::Crystal;
    gold += t.treasure == Treasure::Gold;
    const int e = edges(t.paths);
    if (e == 2 && (t.paths == (kNorth | kSouth) || t.paths == (kEast | kWest))) straights++;
    if (e == 2 && t.paths != (kNorth | kSouth) && t.paths != (kEast | kWest)) bends++;
    tees += e == 3;
    crossings += e == 4;
  }
  check(crystals == 10, "10 tiles have a crystal");
  check(gold == 4, "4 tiles have gold");
  check(straights == 16 && bends == 8 && tees == 8 && crossings == 4, "the set has 16 straights, 8 bends, 8 Ts and 4 crossings");
  check(tile(1).paths == (kEast | kWest) && tile(36).paths == (kNorth | kSouth), "tile 1 runs across and tile 36 down");
  check(tile(14).treasure == Treasure::Gold && tile(3).treasure == Treasure::Crystal, "tile 14 has gold and tile 3 a crystal");
}

void testWholeRound() {
  State s;
  Source source;
  check(s.count == 0 && last(s) == 0 && !finished(s), "a new round has nothing drawn");
  for (int i = 0; i < kTiles; i++) {
    check(draw(s, source, static_cast<uint64_t>(i)), "a draw succeeds while tiles are left");
    check(last(s) >= 1 && last(s) <= kTiles, "a draw is in range");
  }
  check(finished(s), "every tile drawn finishes the round");
  bool seen[kTiles + 1] = {};
  bool repeat = false;
  for (int i = 0; i < s.count; i++) {
    repeat = repeat || seen[s.order[i]];
    seen[s.order[i]] = true;
  }
  check(!repeat, "no tile is drawn twice");
  for (int n = 1; n <= kTiles; n++) check(isDrawn(s, n), "every tile is drawn once");

  const State before = s;
  check(!draw(s, source, 99), "no draw after the last tile");
  check(std::memcmp(&before, &s, sizeof(State)) == 0, "a refused draw changes nothing");
}

// The first draw, from entropy that is only a counter: each tile should come up 1 time in 36.
// Chi-squared with 35 degrees of freedom is under 66.6 at p = 0.001.
void testFirstDrawFair() {
  constexpr int kTrials = 360000;
  std::vector<long> counts(kTiles, 0);
  Source source;
  for (int t = 0; t < kTrials; t++) {
    State s;
    draw(s, source, static_cast<uint64_t>(t));
    counts[last(s) - 1]++;
  }
  check(chiSquared(counts, kTrials / static_cast<double>(kTiles)) < 66.6, "the first draw favours no tile");
}

// Where tile 1 lands in a whole round: every position equally often. Same bound as above.
void testPositionFair() {
  constexpr int kTrials = 72000;
  std::vector<long> counts(kTiles, 0);
  Source source;
  for (int t = 0; t < kTrials; t++) {
    State s;
    while (draw(s, source, static_cast<uint64_t>(t))) {
    }
    for (int i = 0; i < kTiles; i++) {
      if (s.order[i] == 1) counts[i]++;
    }
  }
  check(chiSquared(counts, kTrials / static_cast<double>(kTiles)) < 66.6, "tile 1 lands in every position equally");
}

void testRejection() {
  // 2^32 isn't a multiple of 36, so the top values must be thrown back.
  const uint32_t values[] = {0xFFFFFFFFu, 0xFFFFFFFEu, 7};
  int k = 0;
  const uint32_t r = uniform(36, [&] { return values[k++]; });
  check(k == 3 && r == 7, "values that would favour some tiles are thrown back");
}

void testSave() {
  State s;
  Source source;
  for (int i = 0; i < 20; i++) draw(s, source, static_cast<uint64_t>(i) * 31);
  uint8_t bytes[MAX_SAVE_BYTES];
  const size_t size = pack(s, bytes);
  check(size == SAVE_HEADER + 20, "the save is the header and one byte a draw");

  State loaded;
  check(unpack(SAVE_VERSION, bytes, size, loaded), "a save loads");
  check(std::memcmp(&s, &loaded, sizeof(State)) == 0, "a save loads the same draws in order");

  State full;
  while (draw(full, source, 5)) {
  }
  check(pack(full, bytes) == MAX_SAVE_BYTES, "a finished round fits the largest save");
  check(unpack(SAVE_VERSION, bytes, MAX_SAVE_BYTES, loaded) && finished(loaded), "a finished round loads");

  State untouched;
  draw(untouched, source, 1);
  auto rejects = [&](const uint8_t* in, size_t n, uint8_t version, const char* what) {
    State keep = untouched;
    check(!unpack(version, in, n, keep), what);
    check(std::memcmp(&keep, &untouched, sizeof(State)) == 0, "a refused save changes nothing");
  };
  const uint8_t good[] = {3, 5, 9, 36};
  check(unpack(SAVE_VERSION, good, sizeof(good), loaded), "a small save loads");
  rejects(good, sizeof(good), SAVE_VERSION + 1, "another version is refused");
  rejects(good, sizeof(good) - 1, SAVE_VERSION, "a short save is refused");
  rejects(good, 0, SAVE_VERSION, "a save without its header is refused");
  const uint8_t tooMany[] = {37};
  rejects(tooMany, sizeof(tooMany), SAVE_VERSION, "more draws than tiles is refused");
  const uint8_t outOfRange[] = {2, 5, 37};
  rejects(outOfRange, sizeof(outOfRange), SAVE_VERSION, "a tile past 36 is refused");
  const uint8_t zero[] = {1, 0};
  rejects(zero, sizeof(zero), SAVE_VERSION, "a zero is refused");
  const uint8_t repeat[] = {3, 5, 9, 5};
  rejects(repeat, sizeof(repeat), SAVE_VERSION, "a repeated tile is refused");
}

}  // namespace

int main() {
  testTileSet();
  testWholeRound();
  testFirstDrawFair();
  testPositionFair();
  testRejection();
  testSave();
  if (failures != 0) {
    std::printf("karubatiles: %d failed\n", failures);
    return 1;
  }
  std::printf("karubatiles: all passed\n");
  return 0;
}
