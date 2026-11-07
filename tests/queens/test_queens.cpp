// Queens' rules, flow and saves, checked the way touch-licorice-apps' test_queens.py checks the
// Lua: line parsing, the tap cycle, dots, conflicts, undo, hints, solving by taps in Queens and
// Star Battle, the menu, and save and reload. A solver also checks every bank puzzle has
// exactly one solution, and that it's the one the bank marks.
#include <cstdio>
#include <cstring>
#include <memory>

#include "queens_flow.h"
#include "queens_rules.h"
#include "queens_save.h"

using namespace queens;

namespace {

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    failures++;
  }
}

// Row by row, k pieces a row, with no column or region over k and nothing touching. Counts
// solutions up to 2, keeping the first.
struct Solver {
  const Puzzle& p;
  int colCount[kMaxSize] = {};
  int regionCount[kMaxRegions] = {};
  int regionLeft[kMaxRegions] = {};  // cells of the region in rows not yet reached
  bool piece[kMaxCells] = {};
  bool first[kMaxCells] = {};
  int found = 0;

  explicit Solver(const Puzzle& puzzle) : p(puzzle) {
    for (int i = 0; i < p.n * p.n; i++) regionLeft[p.region[i]]++;
  }

  bool fits(int i) const {
    const int c = i % p.n;
    if (colCount[c] >= p.k || regionCount[p.region[i]] >= p.k) return false;
    bool clear = true;
    around(p.n, i, [&](int j) {
      if (piece[j]) clear = false;
    });
    return clear;
  }

  void row(int r, int from, int left) {
    if (found >= 2) return;
    const int n = p.n;
    if (left == 0) {
      for (int c = 0; c < n; c++) regionLeft[p.region[r * n + c]]--;
      // Every region still needs room for what it lacks.
      bool room = true;
      for (int g = 0; g < n && room; g++) {
        if (regionCount[g] + regionLeft[g] < p.k) room = false;
      }
      if (room) {
        if (r + 1 == n) {
          if (found++ == 0) std::memcpy(first, piece, sizeof(piece));
        } else {
          row(r + 1, 0, p.k);
        }
      }
      for (int c = 0; c < n; c++) regionLeft[p.region[r * n + c]]++;
      return;
    }
    for (int c = from; c < n; c++) {
      const int i = r * n + c;
      if (!fits(i)) continue;
      piece[i] = true;
      colCount[c]++;
      regionCount[p.region[i]]++;
      row(r, c + 2, left - 1);
      piece[i] = false;
      colCount[c]--;
      regionCount[p.region[i]]--;
    }
  }
};

void testParse() {
  Puzzle p;
  check(parseLine(QUEENS_6[0], p) && p.n == 6 && p.k == 1, "a 6x6 bank line parses with one piece a row");
  check(parseLine(STARS_10[0], p) && p.n == 10 && p.k == 2, "a 10x10 Star Battle line has two");
  check(!parseLine("abc", p), "a line that isn't a square is refused");
  check(!parseLine("aaaabbbbccccdd1d", p), "a line with a digit is refused");
  check(!parseLine("aaaabbbbccccdddd", p), "a line with no pieces is refused");
}

void testEveryPuzzle() {
  for (int b = 0; b < kBanks; b++) {
    for (int k = 0; k < kBankPuzzles; k++) {
      Puzzle p;
      char what[64];
      std::snprintf(what, sizeof(what), "%s puzzle %d", kBankList[b].label, k + 1);
      if (!parseLine(kBankList[b].lines[k], p) || p.n != kBankList[b].size || p.k != kBankList[b].pieces) {
        check(false, what);
        continue;
      }
      std::unique_ptr<Solver> solver(new Solver(p));
      solver->row(0, 0, p.k);
      if (solver->found != 1) {
        std::printf("FAIL: %s has %d solutions\n", what, solver->found);
        failures++;
        continue;
      }
      if (std::memcmp(solver->first, p.solution, sizeof(p.solution)) != 0) check(false, what);
    }
  }
}

std::unique_ptr<State> fresh(int bank = 0) {
  std::unique_ptr<State> s(new State());
  s->bank = static_cast<uint8_t>(bank);
  loadPuzzle(*s);
  return s;
}

// Taps a cell until it holds `mark`.
void tapTo(State& s, int i, uint8_t mark) {
  for (int k = 0; k < 3 && s.game.marks[i] != mark; k++) tapCell(s, i);
}

void testQueens() {
  auto s = fresh();
  const int n = s->game.puzzle.n;
  check(!canUndo(*s) && undoTap(*s) == Change::None, "UNDO does nothing with nothing to undo");

  check(tapCell(*s, 0) == Change::Move && s->game.marks[0] == Cross, "one tap is a cross");
  check(tapCell(*s, 0) == Change::Move && s->game.marks[0] == Piece && placed(s->game) == 1, "two a queen");
  bool out[kMaxCells];
  ruledOut(s->game, out);
  check(out[1] && out[n] && out[n + 1] && out[n * (n - 1)] && out[n - 1], "around it, its row and its column are ruled out");
  check(tapCell(*s, 0) == Change::Move && s->game.marks[0] == Empty, "three clears it");

  bool bad[kMaxCells];
  tapTo(*s, 0, Piece);
  tapTo(*s, n + 1, Piece);
  check(conflicts(s->game, bad) == Broken::Touch && bad[0] && bad[n + 1] && s->message == Message::Touch, "two touching are both marked");
  tapTo(*s, n + 1, Empty);
  tapTo(*s, 2, Piece);
  check(conflicts(s->game, bad) == Broken::Row && bad[0] && bad[2] && s->message == Message::Row, "two in a row");
  check(undoTap(*s) == Change::Move && s->game.marks[2] == Cross, "UNDO takes back one tap");
  tapTo(*s, 2, Empty);
  tapTo(*s, 0, Empty);
  check(placed(s->game) == 0, "all cleared");

  // HINT removes a wrong queen, then places a right one, which a tap can't change.
  int wrong = 0;
  while (s->game.puzzle.solution[wrong]) wrong++;
  tapTo(*s, wrong, Piece);
  check(hintTap(*s) == Change::Move && s->game.marks[wrong] == Empty && s->message == Message::HintWrongPiece, "HINT removes a wrong queen");
  check(hintTap(*s) == Change::Move && s->message == Message::HintPlaced && hints(s->game) == 1, "then places a right one");
  int hinted = 0;
  while (!s->game.hinted[hinted]) hinted++;
  check(s->game.puzzle.solution[hinted] && s->game.marks[hinted] == Piece, "where the solution has one");
  check(tapCell(*s, hinted) == Change::View && s->message == Message::HintLocked && s->game.marks[hinted] == Piece, "a hinted queen stays");
  check(tapCell(*s, hinted) == Change::None, "and says so once");

  // A cross where a queen belongs.
  int right = 0;
  while (!s->game.puzzle.solution[right] || right == hinted) right++;
  tapTo(*s, right, Cross);
  check(hintTap(*s) == Change::Move && s->game.marks[right] == Empty && s->message == Message::HintCross, "HINT removes a cross where a queen goes");

  // Undoing a hint takes its mark too, so the cell isn't left locked.
  auto u = fresh();
  hintTap(*u);
  int h = 0;
  while (!u->game.hinted[h]) h++;
  check(undoTap(*u) == Change::Move && !u->game.hinted[h] && u->game.marks[h] == Empty, "UNDO takes back a hint and its mark");
  check(tapCell(*u, h) == Change::Move && u->game.marks[h] == Cross, "so the cell taps again");

  Change last = Change::None;
  for (int i = 0; i < n * n; i++) {
    if (s->game.puzzle.solution[i] && s->game.marks[i] != Piece) {
      for (int k = 0; k < 3 && s->game.marks[i] != Piece; k++) last = tapCell(*s, i);
    }
  }
  check(last == Change::Solved && isSolved(*s) && s->message == Message::Solved, "solved by taps");
  check(isDone(*s, 0, 0) && doneCount(*s, 0) == 1 && s->doneChanged, "and recorded");
  check(!canUndo(*s) && !canHint(*s) && tapCell(*s, 0) == Change::None, "nothing more to do once solved");

  check(stepPuzzle(*s, 1) == Change::NewPuzzle && s->place[0] == 1 && !isSolved(*s) && placed(s->game) == 0, "NEXT opens puzzle 2");
  check(stepPuzzle(*s, -2) == Change::NewPuzzle && s->place[0] == kBankPuzzles - 1, "and back past 1 wraps to 200");
}

void testMenu() {
  auto s = fresh();
  tapCell(*s, 0);
  check(openMore(*s) == Change::View && s->mode == Mode::More, "MORE opens the menu");
  check(tapCell(*s, 1) == Change::None, "the board takes no taps behind it");
  check(chooseBank(*s, 0) == Change::View && s->mode == Mode::Board && s->game.marks[0] == Cross, "the size you're on goes back as it was");
  openMore(*s);
  check(chooseBank(*s, 5) == Change::NewPuzzle && s->bank == 5 && s->game.puzzle.n == 10 && s->game.puzzle.k == 2, "Star Battle 10x10");
  stepPuzzle(*s, 1);
  openMore(*s);
  chooseBank(*s, 0);
  check(s->bank == 0 && s->place[0] == 0 && s->game.marks[0] == Empty, "back to 6x6, at its puzzle, freshly");
  openMore(*s);
  chooseBank(*s, 5);
  check(s->place[5] == 1, "Star Battle kept its place");
  openMore(*s);
  check(toggleDots(*s) == Change::Move && !s->dots && s->mode == Mode::Board, "dots turn off, back on the board");
  tapCell(*s, 0);
  openMore(*s);
  check(restart(*s) == Change::NewPuzzle && placed(s->game) == 0 && s->game.marks[0] == Empty, "START OVER clears the board");
  openMore(*s);
  check(stepPuzzle(*s, -1) == Change::NewPuzzle && s->place[5] == 0 && s->mode == Mode::Board, "PREVIOUS goes back a puzzle");
  check(openRules(*s) == Change::View && closePage(*s) == Change::View && s->mode == Mode::Board, "the rules open and close");
  check(closePage(*s) == Change::None, "BACK on the board isn't a page close");
}

void testStarBattle() {
  auto s = fresh(5);
  const int n = s->game.puzzle.n;
  for (int i = 0; i < n * n; i++) {
    if (!s->game.puzzle.solution[i]) tapCell(*s, i);
  }
  Change last = Change::None;
  for (int i = 0; i < n * n; i++) {
    if (s->game.puzzle.solution[i]) {
      tapCell(*s, i);
      last = tapCell(*s, i);
    }
  }
  check(last == Change::Solved && isSolved(*s) && placed(s->game) == 20, "Star Battle 10x10 solved with 20 stars, crosses and all");
}

void testSave() {
  auto s = fresh(3);
  s->place[0] = 17;
  s->place[3] = 150;
  s->place[5] = 199;
  s->dots = false;
  loadPuzzle(*s);
  tapCell(*s, 0);
  tapCell(*s, 5);
  tapCell(*s, 5);
  hintTap(*s);
  uint8_t data[SAVE_MAX];
  const size_t size = packGame(*s, data);
  check(size == SAVE_HEADER + 21, "a 9x9 save is the header and 81 cells at four a byte");

  auto back = fresh();
  check(unpackGame(SAVE_VERSION, data, size, *back), "the save loads");
  check(back->bank == 3 && back->place[0] == 17 && back->place[3] == 150 && back->place[5] == 199 && !back->dots, "with every place and dots");
  check(std::memcmp(back->game.marks, s->game.marks, sizeof(s->game.marks)) == 0 &&
            std::memcmp(back->game.hinted, s->game.hinted, sizeof(s->game.hinted)) == 0,
        "and the marks, hints and all");
  check(back->game.undoCount == 0, "with no undo, as in the source");

  auto refused = [&](const uint8_t* bytes, size_t n, uint8_t version, const char* what) {
    auto t = fresh();
    t->place[2] = 9;
    check(!unpackGame(version, bytes, n, *t) && t->place[2] == 9 && t->bank == 0, what);
  };
  uint8_t bad[SAVE_MAX];
  refused(data, size, SAVE_VERSION + 1, "another version is refused");
  refused(data, size - 1, SAVE_VERSION, "a short save is refused");
  std::memcpy(bad, data, size);
  bad[0] = kBanks;
  refused(bad, size, SAVE_VERSION, "a bank that doesn't exist is refused");
  std::memcpy(bad, data, size);
  bad[1] = kBankPuzzles;
  refused(bad, size, SAVE_VERSION, "a place past the bank is refused");
  std::memcpy(bad, data, size);
  bad[1 + kBanks] = 2;
  refused(bad, size, SAVE_VERSION, "dots that aren't 0 or 1 are refused");
  std::memcpy(bad, data, size);
  bad[0] = 0;  // a 6x6 needs 9 bytes of marks, not 21
  refused(bad, size, SAVE_VERSION, "marks of the wrong size for the bank are refused");
  std::memcpy(bad, data, size);
  bad[size - 1] |= 0xFC;  // the 81st cell's spare bits
  refused(bad, size, SAVE_VERSION, "spare bits set are refused");
  int wrong = 0;
  while (s->game.puzzle.solution[wrong]) wrong++;
  std::memcpy(bad, data, size);
  bad[SAVE_HEADER + wrong / 4] = static_cast<uint8_t>(bad[SAVE_HEADER + wrong / 4] | (3 << (2 * (wrong % 4))));
  refused(bad, size, SAVE_VERSION, "a hinted queen where none belongs is refused");

  auto done = fresh();
  for (int i = 0; i < 36; i++) {
    if (done->game.puzzle.solution[i]) tapTo(*done, i, Piece);
  }
  stepPuzzle(*done, 199);
  for (int i = 0; i < 36; i++) {
    if (done->game.puzzle.solution[i]) tapTo(*done, i, Piece);
  }
  uint8_t record[DONE_BYTES];
  check(packDone(*done, record) == DONE_BYTES, "the record is every bank");
  auto reloaded = fresh();
  check(unpackDone(DONE_VERSION, record, DONE_BYTES, *reloaded) && isDone(*reloaded, 0, 0) && isDone(*reloaded, 0, 199) &&
            doneCount(*reloaded, 0) == 2,
        "and reloads");
  check(!unpackDone(DONE_VERSION, record, DONE_BYTES - 1, *reloaded), "a short record is refused");
  check(!unpackDone(DONE_VERSION + 1, record, DONE_BYTES, *reloaded), "a record of another version is refused");

  // A solved board reloads solved.
  const size_t doneSize = packGame(*done, data);
  auto again = fresh();
  check(unpackGame(SAVE_VERSION, data, doneSize, *again) && isSolved(*again), "a solved board reloads solved");
}

}  // namespace

int main() {
  testParse();
  testEveryPuzzle();
  testQueens();
  testMenu();
  testStarBattle();
  testSave();
  if (failures) {
    std::printf("%d failure(s)\n", failures);
    return 1;
  }
  std::printf("queens: all passed\n");
  return 0;
}
