// PocketBGG with no panel: the year and plays line, and the list's pages.
#include <cstdio>
#include <cstring>
#include <vector>

#include "pocketbgg_list.h"
#include "pocketbgg_sample.h"

using namespace pocketbgg;

namespace {

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    failures++;
  }
}

bool describes(const Game& game, const char* expected) {
  char out[40];
  describe(game, out, sizeof(out));
  if (std::strcmp(out, expected) == 0) return true;
  std::printf("  got \"%s\", expected \"%s\"\n", out, expected);
  return false;
}

void testDescribe() {
  check(describes({1, "A", 2020, 33}, "2020  -  33 PLAYS"), "a year and plays");
  check(describes({1, "A", 2019, 1}, "2019  -  1 PLAY"), "one play is singular");
  check(describes({1, "A", 2017, 0}, "2017  -  0 PLAYS"), "no plays yet");
  check(describes({1, "A", -1400, 3}, "1400 BC  -  3 PLAYS"), "a year BC");
  check(describes({1, "A", kNoYear, 2}, "2 PLAYS"), "no year leaves the year out");
  check(describes({1, "A", 2020, 65535}, "2020  -  65535 PLAYS"), "the most plays a game can hold");
}

void testPaginate() {
  // Rows of 100 with a gap of 4 in a room of 310: three rows fit (308), a fourth doesn't.
  const std::vector<int> pages = paginate({100, 100, 100, 100, 100, 100, 100}, 310, 4);
  check(pages == std::vector<int>({0, 3, 6}), "three rows a page, and the last page holds the one left");
  check(pageOf(pages, 0) == 0 && pageOf(pages, 2) == 0 && pageOf(pages, 3) == 1 && pageOf(pages, 6) == 2,
        "each row is found on its page");

  check(paginate({100, 100, 100}, 308, 4) == std::vector<int>({0}), "rows that exactly fill the room share a page");
  check(paginate({100, 150, 100, 50}, 260, 4) == std::vector<int>({0, 2}), "rows of different heights");
  check(paginate({}, 300, 4) == std::vector<int>({0}), "no rows gives one empty page");
  check(paginate({50, 400, 50}, 300, 4) == std::vector<int>({0, 1, 2}), "a row too tall for the room gets a page of its own");
}

// The sample has every row on exactly one page, in order, whatever heights the screen measures.
void testSampleCovered() {
  std::vector<int> heights;
  for (int i = 0; i < kSampleCount; i++) heights.push_back(70 + (i % 3) * 42);
  const std::vector<int> pages = paginate(heights, 572, 4);
  bool ordered = pages[0] == 0;
  for (size_t p = 1; p < pages.size(); p++) ordered = ordered && pages[p] > pages[p - 1] && pages[p] < kSampleCount;
  check(ordered && pages.size() > 1, "the sample splits into several pages, each starting after the last");
}

}  // namespace

int main() {
  testDescribe();
  testPaginate();
  testSampleCovered();
  if (failures) {
    std::printf("pocketbgg: %d failed\n", failures);
    return 1;
  }
  std::printf("pocketbgg: all passed\n");
  return 0;
}
