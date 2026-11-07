// Peg Solitaire's rules, flow and save, checked the way touch-licorice-apps'
// test_pegsolitaire.py checks the Lua: a peg that can't jump says so, one that can is selected
// with its targets, jumps and undo, a whole game to one peg in the centre of the English board
// and to one peg on the triangle, and save and reload.
#include <cstdio>
#include <cstring>
#include <set>
#include <vector>

#include "pegsolitaire_flow.h"
#include "pegsolitaire_rules.h"
#include "pegsolitaire_save.h"

using namespace pegsolitaire;

namespace {

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    failures++;
  }
}

int hole(int board, int r, int c) { return cellAt(kBoardList[board], r, c); }

// Depth first, remembering dead positions, as the source's test does. Ends on one peg, in
// `goal` if it's given.
bool solve(Game& g, int goal, std::set<uint64_t>& dead, std::vector<Jump>& path) {
  if (pegCount(g) == 1) return goal == kNoCell || hasPeg(g, goal);
  if (dead.count(g.pegs)) return false;
  for (int i = 0; i < def(g).cells; i++) {
    Jump j[6];
    const int n = jumpsFrom(g, i, j);
    for (int k = 0; k < n; k++) {
      jump(g, j[k].from, j[k].to);
      path.push_back(j[k]);
      if (solve(g, goal, dead, path)) return true;
      path.pop_back();
      undo(g);
    }
  }
  dead.insert(g.pegs);
  return false;
}

std::vector<Jump> solution(int board, int goal) {
  Game g;
  reset(g, board);
  std::set<uint64_t> dead;
  std::vector<Jump> path;
  solve(g, goal, dead, path);
  return path;
}

void testBoards() {
  check(kBoardList[0].cells == 33 && kBoardList[1].cells == 15, "33 and 15 holes");
  State s;
  newGame(s, 0);
  check(pegCount(s.game) == 32 && !hasPeg(s.game, hole(0, 4, 4)), "English starts with the centre empty");
  newGame(s, 1);
  check(pegCount(s.game) == 14 && !hasPeg(s.game, hole(1, 1, 1)), "the triangle starts with the top empty");
}

void testTaps() {
  State s;
  newGame(s, 0);
  check(tapHole(s, hole(0, 1, 3)) == Change::View && s.message == Message::CannotJump && s.sel == kNoCell, "a peg that can't jump says so");
  check(tapHole(s, hole(0, 1, 3)) == Change::None, "and says it once");
  check(tapHole(s, hole(0, 2, 4)) == Change::View && s.sel == hole(0, 2, 4) && s.message == Message::Selected, "one that can is selected");
  Jump j[6];
  check(jumpsFrom(s.game, hole(0, 2, 4), j) == 1 && j[0].to == hole(0, 4, 4), "with its one target, the centre");
  check(tapHole(s, hole(0, 4, 4)) == Change::Move && pegCount(s.game) == 31 && s.game.jumps == 1 && s.sel == kNoCell, "the jump");
  check(tapHole(s, hole(0, 5, 4)) == Change::View, "select a peg that can jump into (3, 4)");
  check(tapHole(s, hole(0, 2, 4)) == Change::View && s.message == Message::PickMarked, "an empty hole that isn't its target");
  check(tapHole(s, hole(0, 2, 4)) == Change::None, "says it once");
  check(tapHole(s, hole(0, 5, 4)) == Change::View && s.sel == kNoCell && s.message == Message::Help, "tapping the selected peg lets it go");
  check(tapHole(s, hole(0, 2, 4)) == Change::None, "an empty hole with nothing selected does nothing");
  tapHole(s, hole(0, 5, 4));
  check(tapOff(s) == Change::View && s.sel == kNoCell && tapOff(s) == Change::None, "a tap off the board lets it go");
  check(undoJump(s) == Change::Move && pegCount(s.game) == 32 && !canUndo(s) && undoJump(s) == Change::None, "UNDO takes the jump back");
  check(!canNew(s) && startAgain(s) == Change::None, "NEW does nothing on an untouched board");
}

void play(State& s, const std::vector<Jump>& path, Change& last) {
  for (const Jump& j : path) {
    tapHole(s, j.from);
    last = tapHole(s, j.to);
  }
}

void testWholeGames() {
  State s;
  newGame(s, 0);
  const std::vector<Jump> english = solution(0, hole(0, 4, 4));
  check(english.size() == 31, "a perfect English game is 31 jumps");
  Change last = Change::None;
  play(s, english, last);
  check(last == Change::Over && isOver(s) && rate(s.game) == Rating::Perfect, "one peg, in the centre: perfect");
  check(s.best[0] == 1 && s.bestChanged, "the best is kept");
  check(tapHole(s, english.back().to) == Change::None, "no taps once it's over");
  check(undoJump(s) == Change::Move && pegCount(s.game) == 2 && !isOver(s), "UNDO from the end");
  check(startAgain(s) == Change::NewGame && pegCount(s.game) == 32, "NEW starts again");

  check(switchBoard(s) == Change::NewGame && s.game.board == 1 && pegCount(s.game) == 14, "the board button switches to the triangle");
  const std::vector<Jump> triangle = solution(1, kNoCell);
  check(triangle.size() == 13, "a triangle game to one peg is 13 jumps");
  s.bestChanged = false;
  play(s, triangle, last);
  check(last == Change::Over && rate(s.game) == Rating::Excellent && s.best[1] == 1 && s.bestChanged, "one peg on the triangle: excellent");

  // A worse finish keeps the best.
  newGame(s, 0);
  s.bestChanged = false;
  std::vector<Jump> stuck;
  Game g = s.game;
  while (anyMove(g)) {
    Jump j[6];
    for (int i = 0; i < def(g).cells; i++) {
      if (jumpsFrom(g, i, j) > 0) {
        jump(g, j[0].from, j[0].to);
        stuck.push_back(j[0]);
        break;
      }
    }
  }
  play(s, stuck, last);
  check(last == Change::Over && pegCount(s.game) > 1 && s.best[0] == 1 && !s.bestChanged, "a worse finish leaves the best alone");

  const Rating r = rate(s.game);
  const int n = pegCount(s.game);
  check((n == 2 && r == Rating::VeryGood) || (n <= 4 && n > 2 && r == Rating::Good) || (n > 4 && r == Rating::TryAgain), "and is rated by its count");
}

void testSave() {
  State s;
  newGame(s, 1);
  s.best[0] = 3;
  s.best[1] = 2;
  const std::vector<Jump> triangle = solution(1, kNoCell);
  for (int k = 0; k < 5; k++) {
    tapHole(s, triangle[k].from);
    tapHole(s, triangle[k].to);
  }
  tapHole(s, triangle[5].from);  // selected, which isn't saved
  uint8_t data[SAVE_MAX];
  const size_t size = pack(s, data);
  check(size == SAVE_HEADER + 10, "five jumps, two bytes each");

  State back;
  newGame(back, 0);
  check(unpack(SAVE_VERSION, data, size, back), "the save loads");
  check(back.game.board == 1 && back.game.pegs == s.game.pegs && back.game.jumps == 5, "with the board and the pegs");
  check(back.best[0] == 3 && back.best[1] == 2 && back.sel == kNoCell, "the bests, and nothing selected");
  check(undoJump(back) == Change::Move && pegCount(back.game) == 10, "and UNDO still works");

  auto refused = [](const uint8_t* bytes, size_t n, uint8_t version, const char* what) {
    State t;
    newGame(t, 0);
    t.best[0] = 7;
    check(!unpack(version, bytes, n, t) && t.game.board == 0 && t.best[0] == 7 && pegCount(t.game) == 32, what);
  };
  uint8_t bad[SAVE_MAX];
  refused(data, size, SAVE_VERSION + 1, "another version is refused");
  refused(data, size - 1, SAVE_VERSION, "a short save is refused");
  std::memcpy(bad, data, size);
  bad[0] = kBoards;
  refused(bad, size, SAVE_VERSION, "a board that doesn't exist is refused");
  std::memcpy(bad, data, size);
  bad[2] = 15;
  refused(bad, size, SAVE_VERSION, "a best the board can't end with is refused");
  std::memcpy(bad, data, size);
  bad[SAVE_HEADER + 1] = bad[SAVE_HEADER];  // a jump onto itself
  refused(bad, size, SAVE_VERSION, "a jump that can't be made is refused");
  std::memcpy(bad, data, size);
  bad[SAVE_HEADER] = 40;
  refused(bad, size, SAVE_VERSION, "a hole that doesn't exist is refused");
}

}  // namespace

int main() {
  testBoards();
  testTaps();
  testWholeGames();
  testSave();
  if (failures) {
    std::printf("%d failure(s)\n", failures);
    return 1;
  }
  std::printf("pegsolitaire: all passed\n");
  return 0;
}
