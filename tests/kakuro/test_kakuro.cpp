// Kakuro with no panel, after touch-licorice-apps' test_kakuro.py: every bank puzzle is filled
// by run reasoning alone (so its solution is the only one), the clues, the ways to make a run,
// playing by taps, crossing a repeated digit and a run that adds up wrong, HINT, levels, and the
// save.
#include <cstdio>
#include <set>
#include <string>
#include <vector>

#include "kakuro_flow.h"
#include "kakuro_rules.h"
#include "kakuro_save.h"

using namespace kakuro;

namespace {

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    failures++;
  }
}

// make_bank.py's run reasoning: a cell keeps the digits some filling of each of its runs, within
// the digits its cells can still take, gives it. Bits 1-9 of a mask are the digits.
bool propagate(const Puzzle& p, uint16_t cand[kMaxCells]) {
  bool changed = true;
  while (changed) {
    changed = false;
    for (const Run& run : p.runs) {
      const size_t len = run.cells.size();
      std::vector<uint16_t> allowed(len, 0);
      std::vector<int> pick(len);
      auto rec = [&](auto&& self, size_t k, int total, uint16_t used) -> void {
        if (k == len) {
          if (total == run.sum) {
            for (size_t j = 0; j < len; j++) allowed[j] |= static_cast<uint16_t>(1 << pick[j]);
          }
          return;
        }
        for (int d = 1; d <= 9 && total + d <= run.sum; d++) {
          if ((used >> d) & 1 || !((cand[run.cells[k]] >> d) & 1)) continue;
          pick[k] = d;
          self(self, k + 1, total + d, static_cast<uint16_t>(used | (1 << d)));
        }
      };
      rec(rec, 0, 0, 0);
      for (size_t j = 0; j < len; j++) {
        if (!allowed[j]) return false;
        if (allowed[j] != cand[run.cells[j]]) {
          cand[run.cells[j]] = allowed[j];
          changed = true;
        }
      }
    }
  }
  return true;
}

bool solvedByReasoning(const Puzzle& p) {
  uint16_t cand[kMaxCells];
  for (int i = 0; i < kMaxCells; i++) cand[i] = 0x3FE;
  if (!propagate(p, cand)) return false;
  for (int i = 0; i < p.n * p.n; i++) {
    if (white(p, i) && cand[i] != (1 << p.solution[i])) return false;
  }
  return true;
}

void testBanks() {
  bool ok = true;
  bool inRuns = true;
  for (int l = 0; l < kLevels; l++) {
    std::set<std::string> seen;
    for (int k = 0; k < kBankPuzzles; k++) {
      Puzzle p;
      if (!parse(level(l).lines[k], level(l).n, p)) ok = false;
      seen.insert(level(l).lines[k]);
      if (!solvedByReasoning(p)) ok = false;
      // Every white cell is in an across run and a down run, as the generator makes them.
      for (int i = 0; i < p.n * p.n; i++) {
        if (white(p, i) && (p.across[i] < 0 || p.down[i] < 0)) inRuns = false;
      }
    }
    if (seen.size() != static_cast<size_t>(kBankPuzzles)) ok = false;
  }
  check(ok, "all 600 puzzles parse, none twice, and run reasoning alone fills each, so it has one solution");
  check(inRuns, "every white cell is in a run each way");

  // And the check can fail: 1+2 across and down both ways round is a square with two fillings.
  Puzzle two;
  check(parse("####" "#12#" "#21#" "####", 4, two) && !solvedByReasoning(two), "a puzzle with two solutions isn't filled by reasoning");
}

void testParse() {
  // The first easy puzzle: ########13####6897#32#12#8415####38#
  Puzzle p;
  parse(kEasy[0], 6, p);
  check(p.solution[8] == 1 && p.solution[9] == 3 && !white(p, 7), "digits and blocks where the line has them");
  check(p.clueAcross[7] == 4 && p.clueDown[2] == 1 + 6 + 2 + 4 && p.clueDown[3] == 3 + 8, "clues are the sums of their runs");
  const Run& r = p.runs[static_cast<size_t>(p.across[8])];
  check(r.across && r.cells.size() == 2 && r.sum == 4, "a white cell knows its across run");
  check(p.outside[0] && !p.outside[7], "an empty block at the edge isn't drawn, a clue cell is");

  check(combos(2, 16) == std::vector<std::string>{"79"}, "16 in 2 is 7 and 9");
  check(combos(2, 10) == (std::vector<std::string>{"19", "28", "37", "46"}), "10 in 2 four ways, no 5 twice");
  check(combos(3, 6) == std::vector<std::string>{"123"} && combos(2, 18).empty(), "6 in 3 one way, 18 in 2 none");
}

void testPlay() {
  State s;
  loadPuzzle(s);
  Board& b = s.board;
  const Puzzle& p = b.puzzle;
  check(b.selected == firstOpen(b) && white(p, b.selected), "the first open cell is selected");
  check(selectCell(s, 0) == Change::None && selectCell(s, b.selected) == Change::None, "a block or the selected cell changes nothing");

  // A digit twice in a run is crossed, both copies.
  const Run& run = p.runs[static_cast<size_t>(p.across[b.selected])];
  const int a = run.cells[0];
  const int c = run.cells[1];
  selectCell(s, a);
  putDigit(s, 9);
  selectCell(s, c);
  check(putDigit(s, 9) == Change::Move, "a digit goes in");
  bool bad[kMaxCells];
  conflicts(b, bad);
  check(bad[a] && bad[c], "a digit twice in a run crosses both");
  check(putDigit(s, 9) == Change::None, "the same digit again changes nothing");
  check(erase(s) == Change::Move && b.entry[c] == 0 && erase(s) == Change::None, "ERASE, then nothing to erase");
  selectCell(s, a);
  erase(s);

  // A full run that adds up wrong.
  for (size_t k = 0; k < run.cells.size(); k++) {
    selectCell(s, run.cells[k]);
    putDigit(s, static_cast<int>(k) + 1 == p.solution[run.cells[k]] ? static_cast<int>(k) + 2 : static_cast<int>(k) + 1);
  }
  int total = 0;
  for (uint8_t x : run.cells) total += b.entry[x];
  conflicts(b, bad);
  check(total == run.sum || bad[run.cells[0]], "a full run that adds up wrong is crossed");

  // HINT fills the selected cell, and that digit stays put.
  selectCell(s, a);
  check(hint(s) == Change::Move && b.entry[a] == p.solution[a] && b.hinted[a] && s.hintDigit == p.solution[a], "HINT fills the selected cell");
  check(putDigit(s, p.solution[a] % 9 + 1) == Change::View && s.message == Message::FromHint && b.entry[a] == p.solution[a],
        "a hinted digit can't be changed, saying why");
  check(erase(s) == Change::None, "and saying so twice changes nothing");

  // The rest typed in solves it.
  Change last = Change::None;
  for (int i = 0; i < p.n * p.n; i++) {
    if (!white(p, i) || b.entry[i] == p.solution[i]) continue;
    selectCell(s, i);
    last = putDigit(s, p.solution[i]);
  }
  check(last == Change::Solved && solved(b) && isDone(s, 0, 0) && b.selected == -1 && hintCount(b) == 1, "solved, counted, one hint");
  check(putDigit(s, 1) == Change::None && hint(s) == Change::None && selectCell(s, a) == Change::None, "a solved board stays as it is");

  // NEXT, PREVIOUS round the ends, levels with their own place.
  check(stepPuzzle(s, -1) == Change::NewPuzzle && s.place[0] == kBankPuzzles - 1 && !solved(b), "before the first is the last");
  stepPuzzle(s, 1);
  check(s.place[0] == 0 && !solved(b), "and back, its digits gone as in the source");
  openMore(s);
  check(chooseLevel(s, 2) == Change::NewPuzzle && b.puzzle.n == 8 && s.view == View::Board, "HARD 8X8");
  stepPuzzle(s, 1);
  openMore(s);
  chooseLevel(s, 0);
  openMore(s);
  chooseLevel(s, 2);
  check(s.place[2] == 1 && s.place[0] == 0, "each level keeps its place");
  openMore(s);
  check(chooseLevel(s, 2) == Change::View && s.view == View::Board, "the level you're on just closes the page");

  // Hint all the way.
  State h;
  loadPuzzle(h);
  Change r = Change::None;
  for (int k = 0; k < 64 && r != Change::Solved; k++) r = hint(h);
  check(r == Change::Solved && hint(h) == Change::None, "HINT alone solves it");
}

void testSave() {
  State s;
  openMore(s);
  chooseLevel(s, 1);
  stepPuzzle(s, 3);
  const int x = s.board.selected;
  putDigit(s, 5);
  const int y = firstOpen(s.board);
  selectCell(s, y);
  hint(s);
  s.done[2][3] = 0x81;
  uint8_t data[SAVE_MAX];
  const size_t size = pack(s, data);
  check(size == SAVE_HEADER + 49, "a 7x7 save");
  State back;
  check(unpack(SAVE_VERSION, data, size, back), "loads back");
  check(back.level == 1 && back.place[1] == 3 && back.board.entry[x] == 5 && back.board.hinted[y] && !back.board.hinted[x] &&
            back.done[2][3] == 0x81,
        "with its level, place, digits, hint and record");

  State keep;
  keep.place[0] = 9;
  auto refused = [&](size_t at, uint8_t v, size_t length) {
    uint8_t bad[SAVE_MAX];
    for (size_t i = 0; i < size; i++) bad[i] = data[i];
    if (at < size) bad[at] = v;
    return !unpack(SAVE_VERSION, bad, length, keep) && keep.place[0] == 9;
  };
  check(!unpack(SAVE_VERSION + 1, data, size, keep), "another version is refused");
  check(refused(size, 0, size - 1) && refused(0, 0, size), "a short one, and one the wrong length for its level");
  check(refused(0, kLevels, size) && refused(2, kBankPuzzles, size), "a level or place out of range");
  int block = -1;
  for (int i = 0; i < 49 && block < 0; i++) {
    if (!white(s.board.puzzle, i)) block = i;
  }
  check(refused(SAVE_HEADER + static_cast<size_t>(block), 3, size), "a digit in a block");
  check(refused(SAVE_HEADER + static_cast<size_t>(x), 10, size) && refused(SAVE_HEADER + static_cast<size_t>(x), 0x25, size), "a digit past 9, stray bits");
  const uint8_t wrong = static_cast<uint8_t>(HINTED | (s.board.puzzle.solution[x] % 9 + 1));
  check(refused(SAVE_HEADER + static_cast<size_t>(x), wrong, size), "a hinted digit that's wrong");
}

}  // namespace

int main() {
  testBanks();
  testParse();
  testPlay();
  testSave();
  if (failures) {
    std::printf("%d failure(s)\n", failures);
    return 1;
  }
  std::printf("kakuro: all passed\n");
  return 0;
}
