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
  expect(pageCount(10) == 1, "ten games fill one page");
  expect(pageCount(11) == 2, "an eleventh game starts page two");
  expect(pageCount(50) == 5, "fifty games are five pages");

  expect(pageOf(0) == 0 && pageOf(9) == 0 && pageOf(10) == 1 && pageOf(49) == 4, "games land on their page");
  expect(firstOnPage(pageOf(13)) == 10, "a page starts at its first game");

  expect(rowsOnPage(0, 50) == 10, "a full page holds ten");
  expect(rowsOnPage(4, 45) == 5, "the last page holds what is left");
  expect(rowsOnPage(5, 50) == 0, "past the end holds nothing");

  expect(stepClamped(0, -1, 50) == 0, "back from the first page stays");
  expect(stepClamped(4, 1, 50) == 4, "forward from the last page stays");
  expect(stepClamped(2, 1, 50) == 3, "forward steps one page");
  expect(stepClamped(2, -1, 50) == 1, "back steps one page");
  expect(stepClamped(0, 1, 3) == 0, "one page never moves");

  expect(pageForSaved(10, 50) == 1, "a saved game reopens its page");
  expect(pageForSaved(-1, 50) == 4, "a removed game reopens the last page");
  expect(pageForSaved(40, 20) == 1, "a list that shrank reopens its last page");
  expect(pageForSaved(-1, 3) == 0, "with one page, that is the only page");

  if (failures != 0) return EXIT_FAILURE;
  std::printf("home pages: all passed\n");
  return EXIT_SUCCESS;
}
