// Nonograms with no panel, after touch-licorice-apps' test_nonogram.py: every bank puzzle is
// line-solvable (so it has one solution) with its clues within the size's cap, decoding, filling,
// crosses by mode and by hold, UNDO, CLEAR, NEXT and the Up and Down buttons, sizes, the puzzles
// kept half done, and both saves.
#include <cstdio>
#include <set>
#include <string>
#include <vector>

#include "nonogram_flow.h"
#include "nonogram_rules.h"
#include "nonogram_save.h"

using namespace nonogram;

namespace {

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    failures++;
  }
}

// make_bank.py's line solver: each line on its own, from its clue and the cells known, over and
// over, with no guessing. A line of up to 12 cells has at most 4096 patterns, so all are tried.
// known: -1 unknown, 0 empty, 1 filled. False if no pattern fits.
// Every pattern of a line of n cells with its clue, worked out once per n.
const std::vector<Clue>& patternClues(int n) {
  static std::vector<Clue> cache[kMaxN + 1];
  std::vector<Clue>& out = cache[n];
  if (out.empty()) {
    for (int m = 0; m < (1 << n); m++) out.push_back(clueOf(n, [&](int k) { return (m >> k) & 1; }));
  }
  return out;
}

bool solveLine(int n, const Clue& clue, std::vector<int>& known) {
  const std::vector<Clue>& clues = patternClues(n);
  bool any = false;
  int all1 = (1 << n) - 1, all0 = (1 << n) - 1;
  for (int m = 0; m < (1 << n); m++) {
    if (clues[static_cast<size_t>(m)] != clue) continue;
    bool fits = true;
    for (int k = 0; k < n && fits; k++) fits = known[static_cast<size_t>(k)] < 0 || known[static_cast<size_t>(k)] == ((m >> k) & 1);
    if (!fits) continue;
    any = true;
    all1 &= m;
    all0 &= ~m;
  }
  if (!any) return false;
  for (int k = 0; k < n; k++) {
    if ((all1 >> k) & 1) known[static_cast<size_t>(k)] = 1;
    if ((all0 >> k) & 1) known[static_cast<size_t>(k)] = 0;
  }
  return true;
}

bool lineSolvable(const Puzzle& p) {
  const int n = p.n;
  std::vector<int> grid(static_cast<size_t>(n * n), -1);
  bool changed = true;
  while (changed) {
    changed = false;
    for (int line = 0; line < 2 * n; line++) {
      const bool row = line < n;
      const int at = row ? line : line - n;
      std::vector<int> known(static_cast<size_t>(n));
      for (int k = 0; k < n; k++) known[static_cast<size_t>(k)] = grid[static_cast<size_t>(row ? at * n + k : k * n + at)];
      const std::vector<int> before = known;
      if (!solveLine(n, row ? p.rows[at] : p.cols[at], known)) return false;
      if (known != before) {
        changed = true;
        for (int k = 0; k < n; k++) grid[static_cast<size_t>(row ? at * n + k : k * n + at)] = known[static_cast<size_t>(k)];
      }
    }
  }
  for (int i = 0; i < n * n; i++) {
    if (grid[static_cast<size_t>(i)] != (p.solution[i] ? 1 : 0)) return false;
  }
  return true;
}

void testBank() {
  bool ok = true;
  for (int s = 0; s < kSizes; s++) {
    std::set<std::string> seen;
    for (int k = 0; k < kBankPuzzles; k++) {
      const char* hex = bankFor(s)[k];
      seen.insert(hex);
      Puzzle p;
      decode(hex, kSizeList[s], p);
      for (int line = 0; line < p.n; line++) {
        if (p.rows[line].empty() || p.cols[line].empty()) ok = false;
        if (static_cast<int>(p.rows[line].size()) > kCap[s] || static_cast<int>(p.cols[line].size()) > kCap[s]) ok = false;
      }
      if (!lineSolvable(p)) ok = false;
    }
    if (seen.size() != static_cast<size_t>(kBankPuzzles)) ok = false;
  }
  check(ok, "all 600 puzzles: line-solvable, so one solution, no empty line, clues within the cap, none twice");

  // And the check can fail: two cells on a diagonal could as well be the other diagonal.
  Puzzle twoWays;
  decode("8200000", 5, twoWays);
  check(twoWays.solution[0] && twoWays.solution[6] && !lineSolvable(twoWays), "a picture with two solutions isn't line-solvable");

  // The first 10x10 decodes as its hex says: 4 bits a digit, top bit first.
  Puzzle p;
  decode(kBank10[0], 10, p);
  const int first = hexValue(kBank10[0][0]);
  check(p.solution[0] == ((first >> 3) & 1) && p.solution[3] == (first & 1), "hex reads top bit first");
}

// Taps every cell of the picture in, as a player would.
Change solveByTaps(State& s) {
  Change last = Change::None;
  const int n = sizeN(s);
  for (int i = 0; i < n * n; i++) {
    if (s.board.puzzle.solution[i] && s.board.cells[i] != Filled) last = tapAt(s, i, false);
  }
  return last;
}

void testPlay() {
  State s;
  loadPuzzle(s);
  check(sizeN(s) == 10 && s.place[2] == 0 && blank(s.board), "opens on 10x10, puzzle 1, blank");

  check(tapAt(s, 0, false) == Change::Move && s.board.cells[0] == Filled && s.board.last == 0, "a tap fills");
  check(tapAt(s, 0, false) == Change::Move && s.board.cells[0] == Unknown, "a second tap clears");
  check(toggleMode(s) == Change::Move && s.cross && s.message == Message::CrossMode, "FILL / X switches to X");
  check(tapAt(s, 1, false) == Change::Move && s.board.cells[1] == Crossed, "a tap in X mode crosses");
  toggleMode(s);
  check(tapAt(s, 2, true) == Change::Move && s.board.cells[2] == Crossed, "a hold crosses in FILL mode");
  check(tapAt(s, 2, true) == Change::Move && s.board.cells[2] == Unknown, "and a second hold clears it");
  tapAt(s, 3, false);
  check(tapAt(s, 3, true) == Change::Move && s.board.cells[3] == Crossed, "a hold on a filled cell crosses it");
  for (int k = 0; k < 7; k++) undoTap(s);
  check(blank(s.board) && undoTap(s) == Change::None && !canUndo(s), "UNDO back to blank, then nothing");

  // A row is done when its filled cells give its clue.
  const int n = sizeN(s);
  for (int c = 0; c < n; c++) {
    if (s.board.puzzle.solution[c]) tapAt(s, c, false);
  }
  check(rowDone(s.board, 0), "a row matching its clue is done");

  check(solveByTaps(s) == Change::Solved && solved(s.board) && s.message == Message::Solved, "the picture tapped in solves it");
  check(isDone(s, 2, 0) && doneCount(s, 2) == 1, "and it counts as solved");
  check(tapAt(s, 0, false) == Change::None && undoTap(s) == Change::None, "a solved board stays as it is");
  check(s.kept[2].empty(), "a solved puzzle isn't kept as half done");
  check(askClear(s) == Change::View && s.view == View::ConfirmClear, "CLEAR asks");
  check(tapAt(s, 0, false) == Change::None && closePage(s) == Change::View && !blank(s.board), "BACK keeps the board");
  askClear(s);
  check(confirmClear(s) == Change::NewPuzzle && blank(s.board) && isDone(s, 2, 0), "and CLEAR clears it, still counted solved");
  check(askClear(s) == Change::None, "nothing to clear on a blank board");

  // NEXT and the buttons, round the ends; half-done puzzles keep their cells.
  tapAt(s, 5, false);
  check(stepPuzzle(s, 1) == Change::NewPuzzle && s.place[2] == 1 && blank(s.board), "NEXT");
  check(s.kept[2].size() == 1 && s.kept[2][0].puzzle == 0, "and the one left is kept");
  stepPuzzle(s, -1);
  check(s.board.cells[5] == Filled && s.board.history.empty(), "back on it, its cells are there, without the undo");
  stepPuzzle(s, -1);
  check(s.place[2] == kBankPuzzles - 1, "before the first is the last");
  stepPuzzle(s, 1);

  // Sizes keep their own place.
  check(openSizes(s) == Change::View && chooseSize(s, 0) == Change::NewPuzzle && sizeN(s) == 5 && s.place[0] == 0, "5x5 from the size page");
  stepPuzzle(s, 1);
  openSizes(s);
  chooseSize(s, 2);
  check(sizeN(s) == 10 && s.place[2] == 0 && s.board.cells[5] == Filled, "and back to 10x10 where it was");
  openSizes(s);
  check(chooseSize(s, 2) == Change::View && s.view == View::Board, "the size you're on just closes the page");

  // At most 40 kept per size, the newest first.
  State many;
  loadPuzzle(many);
  for (int k = 0; k < 45; k++) {
    tapAt(many, 0, true);
    stepPuzzle(many, 1);
  }
  check(many.kept[2].size() == kKeepGrids && many.kept[2][0].puzzle == 44 && many.kept[2].back().puzzle == 5, "the newest 40 kept");

  // Both saves round-trip.
  uint8_t data[SAVE_BYTES];
  const size_t size = packState(s, data);
  State back;
  check(unpackState(SAVE_VERSION, data, size, back) && back.size == 2 && back.place[0] == 1 && isDone(back, 2, 0) && !back.cross,
        "the state loads back");
  const std::vector<uint8_t> grids = packGrids(many);
  check(unpackGrids(grids.data(), grids.size(), back) && back.kept[2].size() == kKeepGrids && back.kept[2][0].cells == many.kept[2][0].cells,
        "and the kept grids");

  // Refused.
  State keep;
  keep.place[0] = 7;
  uint8_t bad[SAVE_BYTES];
  auto refusedState = [&](size_t at, uint8_t v) {
    for (size_t i = 0; i < size; i++) bad[i] = data[i];
    bad[at] = v;
    return !unpackState(SAVE_VERSION, bad, size, keep) && keep.place[0] == 7;
  };
  check(!unpackState(SAVE_VERSION + 1, data, size, keep) && !unpackState(SAVE_VERSION, data, size - 1, keep), "another version, a short one");
  check(refusedState(0, kSizes) && refusedState(1, kBankPuzzles) && refusedState(5, 2), "a size, a place or a mode out of range");
  check(refusedState(6 + kDoneBytes - 1, 0x40), "a solved bit past the last puzzle");

  auto refusedGrids = [&](std::vector<uint8_t> g) {
    State u;
    u.kept[1].push_back(Kept{3, std::vector<uint8_t>(64, 1)});
    return !unpackGrids(g.data(), g.size(), u) && u.kept[1].size() == 1;
  };
  State one;
  loadPuzzle(one);
  tapAt(one, 0, true);
  std::vector<uint8_t> g = packGrids(one);
  // [version, 0 (5x5), 0 (8x8), 1, puzzle, 25 bytes of cells, 0 (12x12)]
  check(g.size() == 1 + 4 + 1 + 25 && g[3] == 1 && g[4] == 0 && g[5] == Crossed, "a kept 10x10 grid packs as expected");
  std::vector<uint8_t> b = g;
  b.pop_back();
  check(refusedGrids(b), "a short grids save");
  b = g;
  b[5] = 3;
  check(refusedGrids(b), "a cell value 3");
  b = g;
  b[5] = 0;
  check(refusedGrids(b), "a blank grid");
  b = g;
  b[4] = kBankPuzzles;
  check(refusedGrids(b), "a puzzle past the bank");
  b = g;
  b[3] = 2;
  b.insert(b.begin() + 30, g.begin() + 4, g.begin() + 30);
  check(refusedGrids(b), "the same puzzle twice");
  State solvedOne;
  loadPuzzle(solvedOne);
  for (int i = 0; i < 100; i++) solvedOne.board.cells[i] = solvedOne.board.puzzle.solution[i] ? Filled : Unknown;
  solvedOne.kept[2].push_back(Kept{0, std::vector<uint8_t>(solvedOne.board.cells, solvedOne.board.cells + 100)});
  b = packGrids(solvedOne);
  check(refusedGrids(b), "a solved grid kept as half done");
}

}  // namespace

int main() {
  testBank();
  testPlay();
  if (failures) {
    std::printf("%d failure(s)\n", failures);
    return 1;
  }
  std::printf("nonogram: all passed\n");
  return 0;
}
