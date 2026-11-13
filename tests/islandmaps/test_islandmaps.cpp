// Island Maps with no panel: every island is a clean grid of the right size and tile count, in one
// piece, with a unique name; stepping wraps; the random pick never repeats the current island and
// favours none; and the save.
#include <cstdio>
#include <cstring>
#include <vector>

#include "islandmaps_maps.h"
#include "islandmaps_rules.h"
#include "islandmaps_save.h"

using namespace islandmaps;

namespace {

int failures = 0;

void check(bool ok, const char* what, const char* which = "") {
  if (!ok) {
    std::printf("FAIL: %s %s\n", what, which);
    failures++;
  }
}

// The PDF's own counts for the four that aren't 24.
int expectedTiles(const char* name) {
  if (std::strcmp(name, "CORAL REEF") == 0 || std::strcmp(name, "PALM SPRINGS") == 0) return 25;
  if (std::strcmp(name, "HANGMAN'S HARBOUR") == 0) return 26;
  if (std::strcmp(name, "SERPENT SHOALS") == 0) return 22;
  return 24;
}

void testIslands() {
  check(kIslandCount == 55, "there are 55 islands");
  check(std::strcmp(kIslands[0].name, "FORBIDDEN ISLAND") == 0, "Forbidden Island comes first");
  for (int i = 0; i < kIslandCount; i++) {
    const Island& island = kIslands[i];
    const int w = island.width;
    const int h = island.height;
    check(w >= 1 && w <= kMaxWidth && h >= 1 && h <= kMaxHeight, "fits the largest grid", island.name);
    check(static_cast<int>(std::strlen(island.cells)) == w * h, "has width * height cells", island.name);
    check(std::strlen(island.name) <= 22, "has a name short enough to fit", island.name);

    int tiles = 0;
    bool clean = true;
    for (int k = 0; k < w * h; k++) {
      clean = clean && (island.cells[k] == '#' || island.cells[k] == '.');
      tiles += island.cells[k] == '#';
    }
    check(clean, "holds only # and .", island.name);
    check(tiles == expectedTiles(island.name), "has its tile count", island.name);

    auto rowUsed = [&](int r) { for (int c = 0; c < w; c++) if (isLand(island, c, r)) return true; return false; };
    auto columnUsed = [&](int c) { for (int r = 0; r < h; r++) if (isLand(island, c, r)) return true; return false; };
    check(rowUsed(0) && rowUsed(h - 1) && columnUsed(0) && columnUsed(w - 1), "has no empty edge", island.name);

    // One piece, counting tiles that touch only at a corner, as X Marks the Spot's do.
    std::vector<int> seen(w * h, 0), todo;
    for (int k = 0; k < w * h && todo.empty(); k++) {
      if (island.cells[k] == '#') {
        todo.push_back(k);
        seen[k] = 1;
      }
    }
    int reached = 0;
    while (!todo.empty()) {
      const int k = todo.back();
      todo.pop_back();
      reached++;
      for (int dr = -1; dr <= 1; dr++) {
        for (int dc = -1; dc <= 1; dc++) {
          const int r = k / w + dr;
          const int c = k % w + dc;
          if (r < 0 || r >= h || c < 0 || c >= w || seen[r * w + c] || !isLand(island, c, r)) continue;
          seen[r * w + c] = 1;
          todo.push_back(r * w + c);
        }
      }
    }
    check(reached == tiles, "is in one piece", island.name);

    for (int j = 0; j < i; j++) check(std::strcmp(kIslands[j].name, island.name) != 0, "has a unique name", island.name);
  }
}

void testStep() {
  check(step(0, -1, 55) == 54, "Up from the first goes to the last");
  check(step(54, 1, 55) == 0, "Down from the last goes to the first");
  check(step(10, 1, 55) == 11 && step(10, -1, 55) == 9, "Up and Down step by one");
}

void testRandom() {
  // Every other island equally often, from a plain counter's worth of bits. Chi-squared with 53
  // degrees of freedom is under 90.6 at p = 0.001.
  constexpr int kTrials = 270000;
  std::vector<long> counts(kIslandCount, 0);
  uint32_t state = 12345;
  auto next = [&] {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
  };
  const int current = 7;
  bool repeated = false;
  for (int t = 0; t < kTrials; t++) {
    const int pick = pickOther(current, kIslandCount, next);
    repeated = repeated || pick == current || pick < 0 || pick >= kIslandCount;
    if (pick >= 0 && pick < kIslandCount) counts[pick]++;
  }
  check(!repeated, "a random pick is never the island on screen");
  double chi = 0;
  const double expected = kTrials / static_cast<double>(kIslandCount - 1);
  for (int i = 0; i < kIslandCount; i++) {
    if (i == current) continue;
    chi += (counts[i] - expected) * (counts[i] - expected) / expected;
  }
  check(chi < 90.6, "a random pick favours no island");

  const uint32_t values[] = {0xFFFFFFFFu, 5};
  int k = 0;
  check(uniform(54, [&] { return values[k++]; }) == 5 && k == 2, "values that would favour some islands are thrown back");
  check(pickOther(0, 1, next) == 0, "a list of one has nothing else to pick");
}

void testSave() {
  uint8_t bytes[SAVE_BYTES];
  int loaded = 0;
  check(pack(54, bytes) == SAVE_BYTES, "the save is one byte");
  check(unpack(SAVE_VERSION, bytes, SAVE_BYTES, loaded) && loaded == 54, "a save loads its island");

  int keep = 3;
  const uint8_t past[] = {55};
  check(!unpack(SAVE_VERSION, past, 1, keep) && keep == 3, "an island past the list is refused");
  check(!unpack(SAVE_VERSION + 1, bytes, 1, keep) && keep == 3, "another version is refused");
  const uint8_t two[] = {1, 2};
  check(!unpack(SAVE_VERSION, two, 2, keep) && keep == 3, "a save of the wrong size is refused");
}

}  // namespace

int main() {
  testIslands();
  testStep();
  testRandom();
  testSave();
  if (failures != 0) {
    std::printf("islandmaps: %d failed\n", failures);
    return 1;
  }
  std::printf("islandmaps: all passed\n");
  return 0;
}
