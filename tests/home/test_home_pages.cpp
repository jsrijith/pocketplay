// The home page's paging rules, checked on the PC.
#include <cstdio>
#include <cstdlib>

#include "home_pages.h"

namespace {

int failures = 0;

void expect(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    failures++;
  }
}

}  // namespace

int main() {
  using namespace homePages;

  expect(pageCount(0) == 1, "no games is still one page");
  expect(pageCount(1) == 1, "one game is one page");
  expect(pageCount(8) == 1, "eight games fill one page");
  expect(pageCount(9) == 2, "a ninth game starts page two");
  expect(pageCount(50) == 7, "fifty games are seven pages");

  expect(pageOf(0) == 0 && pageOf(7) == 0 && pageOf(8) == 1 && pageOf(49) == 6, "games land on their page");
  expect(firstOnPage(pageOf(13)) == 8, "a page starts at its first game");

  expect(rowsOnPage(0, 50) == 8, "a full page holds eight");
  expect(rowsOnPage(6, 50) == 2, "the last page holds what is left");
  expect(rowsOnPage(7, 50) == 0, "past the end holds nothing");

  expect(stepClamped(0, -1, 50) == 0, "back from the first page stays");
  expect(stepClamped(6, 1, 50) == 6, "forward from the last page stays");
  expect(stepClamped(2, 1, 50) == 3, "forward steps one page");
  expect(stepClamped(2, -1, 50) == 1, "back steps one page");
  expect(stepClamped(0, 1, 3) == 0, "one page never moves");

  expect(pageForSaved(10, 50) == 1, "a saved game reopens its page");
  expect(pageForSaved(-1, 50) == 6, "a removed game reopens the last page");
  expect(pageForSaved(40, 20) == 2, "a list that shrank reopens its last page");
  expect(pageForSaved(-1, 3) == 0, "with one page, that is the only page");

  if (failures != 0) return EXIT_FAILURE;
  std::printf("home pages: all passed\n");
  return EXIT_SUCCESS;
}
