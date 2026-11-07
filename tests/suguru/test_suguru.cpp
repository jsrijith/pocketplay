// Suguru's rules, flow and saves, checked the way touch-licorice-apps' test_suguru.py
// checks the Lua: line parsing, the three conflict rules, every bank puzzle having one
// solution, solving through taps, the buttons, and save and reload.
#include <cstdio>
#include <cstring>

#include "suguru_flow.h"
#include "suguru_rules.h"
#include "suguru_save.h"

using namespace suguru;

namespace {

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    failures++;
  }
}

// A backtracking solver, only for the test: it counts solutions up to `limit`, always
// filling the empty cell with the fewest numbers left. Each cell lists the cells it may
// not repeat (its region and its touching cells), so a candidate check is one pass over
// that list rather than the grid.
struct Solver {
  const Puzzle& p;
  uint8_t cells[kMaxCells];
  uint8_t solution[kMaxCells];
  int size[kMaxCells];
  int peers[kMaxCells][kMaxCells];
  int peerCount[kMaxCells] = {};
  int found = 0;
  int limit = 2;

  explicit Solver(const Puzzle& puzzle) : p(puzzle) {
    const int n = p.n;
    for (int i = 0; i < n * n; i++) {
      cells[i] = p.givens[i];
      size[i] = regionSize(p, i);
      for (int j = 0; j < n * n; j++) {
        const int dr = j / n - i / n;
        const int dc = j % n - i % n;
        const bool touching = dr >= -1 && dr <= 1 && dc >= -1 && dc <= 1;
        if (j != i && (touching || p.regions[j] == p.regions[i])) peers[i][peerCount[i]++] = j;
      }
    }
  }

  // Bit v set: v is still open for cell i.
  unsigned open(int i) const {
    unsigned mask = ((1u << (size[i] + 1)) - 1) & ~1u;
    for (int k = 0; k < peerCount[i]; k++) mask &= ~(1u << cells[peers[i][k]]);
    return mask;
  }

  void run() {
    if (found >= limit) return;
    int best = -1;
    unsigned bestMask = 0;
    int bestCount = 99;
    for (int i = 0; i < p.n * p.n; i++) {
      if (cells[i]) continue;
      const unsigned mask = open(i);
      const int count = __builtin_popcount(mask);
      if (count < bestCount) {
        best = i;
        bestMask = mask;
        bestCount = count;
        if (count == 0) return;
      }
    }
    if (best < 0) {
      if (found == 0) std::memcpy(solution, cells, sizeof(cells));
      found++;
      return;
    }
    for (uint8_t v = 1; v <= kMaxDigit; v++) {
      if (!(bestMask & (1u << v))) continue;
      cells[best] = v;
      run();
      cells[best] = 0;
    }
  }
};

void testParseLine() {
  Puzzle p;
  char line[64];
  std::memset(line, 'A', 25);
  std::memset(line + 25, '0', 25);
  line[50] = 0;
  check(parseLine(line, p) && p.n == 5 && p.regions[24] == 'A' && p.givens[0] == 0, "an empty 5x5 parses");
  line[25] = '6';
  check(!parseLine(line, p), "a 6 never fits a region");
  check(!parseLine("garbage", p), "garbage is refused");
  check(!parseLine("AAAAAAAAA000000000", p), "a 3x3 is too small");
  std::memset(line, 'A', 25);
  line[3] = '-';
  line[25] = '0';
  check(!parseLine(line, p), "a region letter must be a letter or digit");
}

Puzzle rows5() {
  Puzzle p;
  p.n = 5;
  const char* regions = "AAAAABBBBBCCCCCDDDDDEEEEE";
  for (int i = 0; i < 25; i++) p.regions[i] = regions[i];
  return p;
}

void testConflicts() {
  bool bad[kMaxCells];
  Puzzle p = rows5();
  uint8_t cells[kMaxCells] = {};

  Puzzle single = p;
  single.regions[0] = 'F';
  cells[0] = 2;
  check(conflicts(single, cells, bad) == 1 && bad[0], "a 2 is too big for a region of one");

  std::memset(cells, 0, sizeof(cells));
  cells[0] = 1;
  cells[2] = 1;
  check(conflicts(p, cells, bad) == 2 && bad[0] && bad[2], "a repeat in a region");

  std::memset(cells, 0, sizeof(cells));
  cells[0] = 1;
  cells[6] = 1;
  check(conflicts(p, cells, bad) == 2 && bad[0] && bad[6], "the same number touching diagonally");

  std::memset(cells, 0, sizeof(cells));
  cells[0] = 1;
  cells[12] = 1;
  check(conflicts(p, cells, bad) == 0, "the same number apart, in other regions, is fine");
}

void testEveryPuzzle() {
  for (int b = 0; b < kBanks; b++) {
    for (int k = 0; k < kBankPuzzles; k++) {
      Puzzle p;
      char what[64];
      std::snprintf(what, sizeof(what), "%s puzzle %d", kBankList[b].label, k + 1);
      if (!parseLine(kBankList[b].lines[k], p) || p.n != kBankList[b].size) {
        check(false, what);
        continue;
      }
      Solver solver(p);
      solver.run();
      if (solver.found != 1) {
        std::printf("FAIL: %s has %d solutions\n", what, solver.found);
        failures++;
        continue;
      }
      if (!solved(p, solver.solution)) check(false, what);
    }
  }
}

// The flow's answer for the puzzle on the board.
void solutionOf(const State& s, uint8_t* out) {
  Solver solver(s.puzzle);
  solver.limit = 1;
  solver.run();
  std::memcpy(out, solver.solution, sizeof(solver.solution));
}

void play(State& s) {
  uint8_t sol[kMaxCells];
  solutionOf(s, sol);
  int empty = 0;
  while (isGiven(s, empty)) empty++;

  check(select(s, empty) == Change::View && s.sel == empty, "a tap selects a cell");
  check(select(s, empty) == Change::None, "tapping the selected cell again changes nothing");
  const uint8_t wrong = sol[empty] == 1 ? 2 : 1;
  check(setCell(s, wrong) == Change::Move && s.cells[empty] == wrong, "a wrong number goes in");
  check(setCell(s, wrong) == Change::None, "the same number again changes nothing");
  check(canErase(s) && setCell(s, 0) == Change::Move && s.cells[empty] == 0, "ERASE clears it");
  check(!canErase(s) && setCell(s, 0) == Change::None, "ERASE on an empty cell changes nothing");

  Change last = Change::None;
  for (int i = 0; i < cellCount(s); i++) {
    if (isGiven(s, i)) continue;
    select(s, i);
    last = setCell(s, sol[i]);
  }
  check(last == Change::Solved && s.solved, "the last right number solves it");
  check(!canEnter(s) && setCell(s, 1) == Change::None, "the pad does nothing once solved");

  int given = 0;
  while (!isGiven(s, given)) given++;
  check(select(s, given) == Change::View, "a given can be selected");
  check(!canEnter(s), "but not changed");
}

void testFlow() {
  State s;
  loadPuzzle(s);
  check(s.puzzle.n == 5 && s.sel == kNoCell && !s.solved && s.check, "a fresh state is 5x5 puzzle 1, check on");
  check(setCell(s, 3) == Change::View && s.message == Message::CellFirst, "a number with no cell asks for a cell");
  check(setCell(s, 3) == Change::None, "asking again changes nothing");

  int given = 0;
  while (!isGiven(s, given)) given++;
  select(s, given);
  check(setCell(s, 1) == Change::View && s.message == Message::Given, "a given can't change");
  check(s.cells[given] == s.puzzle.givens[given], "and keeps its number");

  play(s);

  // A number that breaks a rule is crossed.
  step(s, 1);
  int empty = 0;
  while (isGiven(s, empty)) empty++;
  int clash = -1;
  for (int j = 0; j < cellCount(s); j++) {
    if (j != empty && isGiven(s, j) && s.puzzle.regions[j] == s.puzzle.regions[empty]) clash = j;
  }
  if (clash >= 0) {
    select(s, empty);
    setCell(s, s.puzzle.givens[clash]);
    bool bad[kMaxCells];
    conflicts(s.puzzle, s.cells, bad);
    check(bad[empty] && bad[clash], "a repeat in a region crosses both cells");
  }

  for (int b = 0; b < 3; b++) check(nextBank(s) == Change::NewPuzzle, "BANK moves to the next bank");
  check(s.bank == 3 && s.puzzle.n == 8 && s.place[3] == 0, "three BANKs reach 8x8 expert, at its first puzzle");
  play(s);

  check(toggleCheck(s) == Change::Move && !s.check, "check turns off");
  toggleCheck(s);
  check(s.check, "and on again");

  step(s, 1);
  check(s.place[3] == 1 && !s.solved && s.sel == kNoCell, "NEXT shows puzzle 2, fresh");
  step(s, 1);
  step(s, -1);
  check(s.place[3] == 1, "Down then Up returns");
  nextBank(s);
  check(s.bank == 0 && s.place[0] == 1, "BANK wraps to 5x5, which kept its place");
  nextBank(s);
  nextBank(s);
  nextBank(s);
  check(s.bank == 3 && s.place[3] == 1, "and back to 8x8, still at puzzle 2");
  step(s, -1);
  step(s, -1);
  check(s.place[3] == kBankPuzzles - 1, "Up from puzzle 1 wraps to puzzle 200");
  step(s, 1);
  check(s.place[3] == 0, "and Down from 200 to puzzle 1");
}

void testSave() {
  State s;
  s.bank = 1;
  s.place[0] = 9;
  s.place[1] = 41;
  s.place[3] = 199;
  s.check = false;
  loadPuzzle(s);
  int empty = 0;
  while (isGiven(s, empty)) empty++;
  select(s, empty);
  setCell(s, 2);

  uint8_t data[SAVE_MAX];
  const size_t size = pack(s, data);
  check(size == SAVE_HEADER + 36, "a 6x6 save is the header and 36 cells");

  State back;
  check(unpack(SAVE_VERSION, data, size, back), "the save loads");
  check(back.bank == 1 && back.place[0] == 9 && back.place[1] == 41 && back.place[3] == 199 && !back.check,
        "with the bank, every place and check");
  check(std::memcmp(back.cells, s.cells, sizeof(s.cells)) == 0 && back.puzzle.n == 6, "and the grid");
  check(back.sel == kNoCell, "with nothing selected");

  State untouched;
  untouched.place[2] = 7;
  auto refused = [&](const uint8_t* bytes, size_t n, uint8_t version, const char* what) {
    State t = untouched;
    check(!unpack(version, bytes, n, t) && t.place[2] == 7, what);
  };
  uint8_t bad[SAVE_MAX];
  refused(data, size, SAVE_VERSION + 1, "another version is refused");
  refused(data, size - 1, SAVE_VERSION, "a short save is refused");
  refused(data, 3, SAVE_VERSION, "a save shorter than its header is refused");
  std::memcpy(bad, data, size);
  bad[0] = kBanks;
  refused(bad, size, SAVE_VERSION, "a bank that doesn't exist is refused");
  std::memcpy(bad, data, size);
  bad[2] = kBankPuzzles;
  refused(bad, size, SAVE_VERSION, "a place past the bank is refused");
  std::memcpy(bad, data, size);
  bad[5] = 2;
  refused(bad, size, SAVE_VERSION, "a check that isn't 0 or 1 is refused");
  std::memcpy(bad, data, size);
  bad[SAVE_HEADER + empty] = 6;
  refused(bad, size, SAVE_VERSION, "a 6 in a cell is refused");
  int given = 0;
  while (!isGiven(s, given)) given++;
  std::memcpy(bad, data, size);
  bad[SAVE_HEADER + given] = s.puzzle.givens[given] == 1 ? 2 : 1;
  refused(bad, size, SAVE_VERSION, "a changed given is refused");
  std::memcpy(bad, data, size);
  bad[0] = 3;  // an 8x8 bank needs 64 cells, not 36
  refused(bad, size, SAVE_VERSION, "a grid of the wrong size for its bank is refused");

  // A solved grid reloads as solved, so the pad stays shut.
  State done;
  loadPuzzle(done);
  uint8_t sol[kMaxCells];
  solutionOf(done, sol);
  std::memcpy(done.cells, sol, sizeof(sol));
  const size_t doneSize = pack(done, data);
  State reloaded;
  check(unpack(SAVE_VERSION, data, doneSize, reloaded) && reloaded.solved, "a solved grid reloads solved");
}

}  // namespace

int main() {
  testParseLine();
  testConflicts();
  testEveryPuzzle();
  testFlow();
  testSave();
  if (failures) {
    std::printf("%d failure(s)\n", failures);
    return 1;
  }
  std::printf("suguru: all passed\n");
  return 0;
}
