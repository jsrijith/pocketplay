// Grid Gambit's rules, generator, flow and save: how each piece captures, that every puzzle the
// generator makes can be solved and fits its level, the win chance that grades them, a whole game won and one lost, undo, and
// save and reload.
#include <chrono>
#include <cstdio>
#include <cstring>
#include <vector>
#include <algorithm>

#include "gridgambit_flow.h"
#include "gridgambit_rules.h"
#include "gridgambit_save.h"

using namespace gridgambit;

namespace {

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    failures++;
  }
}

int at(int r, int c) { return r * kSide + c; }

bool captures(const Board& b, int from, int to) { return canCapture(b, from, to); }

void testMoves() {
  // A rook sees along its row and column up to the first piece, and no further.
  Board b;
  b.cell[at(3, 0)] = Rook;
  b.cell[at(1, 0)] = Pawn;
  b.cell[at(0, 0)] = Knight;
  b.cell[at(3, 3)] = Bishop;
  check(captures(b, at(3, 0), at(1, 0)), "rook takes up its column");
  check(!captures(b, at(3, 0), at(0, 0)), "rook is blocked");
  check(captures(b, at(3, 0), at(3, 3)), "rook takes along its row");
  int t[8];
  check(capturesFrom(b, at(3, 0), t) == 2, "rook has two captures");

  // A bishop only on diagonals, blocked by the first piece.
  b = Board{};
  b.cell[at(3, 0)] = Bishop;
  b.cell[at(2, 1)] = Pawn;
  b.cell[at(0, 3)] = Rook;
  b.cell[at(3, 2)] = Rook;
  check(captures(b, at(3, 0), at(2, 1)), "bishop takes diagonally");
  check(!captures(b, at(3, 0), at(0, 3)), "bishop is blocked");
  check(!captures(b, at(3, 0), at(3, 2)), "bishop doesn't take along a row");

  // A queen does both.
  b.cell[at(3, 0)] = Queen;
  check(captures(b, at(3, 0), at(3, 2)) && captures(b, at(3, 0), at(2, 1)), "queen takes both ways");

  // A knight jumps over pieces.
  b = Board{};
  b.cell[at(0, 0)] = Knight;
  b.cell[at(0, 1)] = Pawn;
  b.cell[at(1, 0)] = Pawn;
  b.cell[at(1, 2)] = Rook;
  b.cell[at(2, 1)] = Rook;
  b.cell[at(1, 1)] = Rook;
  check(captures(b, at(0, 0), at(1, 2)) && captures(b, at(0, 0), at(2, 1)), "knight jumps");
  check(!captures(b, at(0, 0), at(1, 1)), "knight doesn't take next to it");

  // A king one square any way.
  b = Board{};
  b.cell[at(1, 1)] = King;
  b.cell[at(2, 2)] = Pawn;
  b.cell[at(3, 3)] = Pawn;
  check(captures(b, at(1, 1), at(2, 2)) && !captures(b, at(1, 1), at(3, 3)), "king takes one square");

  // A pawn only diagonally up, never straight or down.
  b = Board{};
  b.cell[at(2, 1)] = Pawn;
  b.cell[at(1, 0)] = Rook;
  b.cell[at(1, 2)] = Rook;
  b.cell[at(1, 1)] = Rook;
  b.cell[at(3, 0)] = Rook;
  check(captures(b, at(2, 1), at(1, 0)) && captures(b, at(2, 1), at(1, 2)), "pawn takes up-diagonal");
  check(!captures(b, at(2, 1), at(1, 1)), "pawn doesn't take straight");
  check(!captures(b, at(2, 1), at(3, 0)), "pawn doesn't take downwards");
  b = Board{};
  b.cell[at(0, 1)] = Pawn;
  b.cell[at(1, 0)] = Rook;
  check(capturesFrom(b, at(0, 1), t) == 0, "pawn on the top row is stuck");

  // Nothing moves to an empty square, and a capture swaps nothing else.
  b = Board{};
  b.cell[at(0, 0)] = Rook;
  check(!captures(b, at(0, 0), at(0, 3)), "no move onto an empty square");
  b.cell[at(0, 3)] = Knight;
  check(capture(b, at(0, 0), at(0, 3)) == Knight, "capture returns what it took");
  check(b.cell[at(0, 3)] == Rook && b.cell[at(0, 0)] == None, "capture moves the piece");
  check(capture(b, at(0, 3), at(0, 0)) == None && b.cell[at(0, 3)] == Rook, "refused capture changes nothing");
  check(isWon(b) && !isLost(b), "one piece wins");
}

void testSolver() {
  Board b;
  b.cell[at(3, 0)] = Rook;
  b.cell[at(0, 0)] = Pawn;
  check(countWins(b, 100) == 1, "rook takes pawn: one way");
  b = Board{};
  b.cell[at(3, 0)] = Rook;
  b.cell[at(3, 3)] = Rook;
  check(countWins(b, 100) == 2, "two rooks: either takes");
  b = Board{};
  b.cell[at(0, 0)] = Pawn;
  b.cell[at(3, 3)] = Pawn;
  check(countWins(b, 100) == 0 && isLost(b), "two stuck pawns lose");
  b.cell[at(1, 1)] = Knight;  // can't reach either corner pawn
  check(countWins(b, 100) == 0, "knight can't reach");
}

Memo makeMemo(std::vector<uint64_t>& slots) {
  slots.assign(8192, 0);
  Memo m;
  m.slots = slots.data();
  m.mask = static_cast<uint32_t>(slots.size() - 1);
  return m;
}

void testWinChance() {
  std::vector<uint64_t> slots;
  Memo memo = makeMemo(slots);
  Board b;
  b.cell[at(3, 0)] = Rook;
  b.cell[at(0, 0)] = Pawn;
  check(winChance(b, memo) == 65535, "only move wins: certain");
  b.cell[at(1, 3)] = Pawn;  // the rook's only capture leaves it facing a pawn it can't reach
  memo.clear();
  check(winChance(b, memo) == 0 && countWins(b, 10) == 0, "no win: zero chance");
  // Three first moves: the rook takes either pawn and then the bishop (both win), or the
  // bishop takes the rook and is left with nothing (loses). Two in three.
  b = Board{};
  b.cell[at(3, 0)] = Rook;
  b.cell[at(0, 0)] = Pawn;
  b.cell[at(3, 3)] = Pawn;
  b.cell[at(0, 3)] = Bishop;
  memo.clear();
  check(winChance(b, memo) == 43690 && countWins(b, 10) == 2, "two of three first moves win");
  // A tiny memo still gives the same answers, only slower.
  std::vector<uint64_t> small(2, 0);
  Memo tiny{small.data(), 1};
  Rng rng(99);
  Board p = generate(rng, 3, memo);
  Board q = p;
  memo.clear();
  check(winChance(p, memo) == winChance(q, tiny), "a full memo gives the same chance");
}

void testGenerator() {
  std::vector<uint64_t> slots;
  Memo memo = makeMemo(slots);
  // Fewer of the bigger puzzles, which take longest under the sanitizers.
  const int perLevelList[kLevels] = {150, 120, 80, 25};
  double slowest = 0;
  for (int level = 0; level < kLevels; level++) {
    const LevelDef& def = kLevelList[level];
    const int perLevel = perLevelList[level];
    int sizes[kMaxPieces + 1] = {};
    std::vector<uint32_t> chances;
    double total = 0;
    for (int seed = 1; seed <= perLevel; seed++) {
      Rng rng(static_cast<uint32_t>(seed * 2654435761u + level));
      const auto t0 = std::chrono::steady_clock::now();
      Board b = generate(rng, level, memo);
      const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
      total += ms;
      if (ms > slowest) slowest = ms;
      const int n = pieceCount(b);
      if (n < def.minPieces || n > def.maxPieces) {
        std::printf("FAIL: level %d seed %d made %d pieces\n", level, seed, n);
        failures++;
        continue;
      }
      sizes[n]++;
      Board copy = b;
      if (countWins(copy, 1) == 0) {
        std::printf("FAIL: level %d seed %d is unsolvable\n", level, seed);
        failures++;
      }
      memo.clear();
      chances.push_back(winChance(b, memo));
      int per[kPieceTypes] = {};
      for (int i = 0; i < kCells; i++) per[b.cell[i]]++;
      for (int p = 1; p < kPieceTypes; p++) {
        if (per[p] > kPieceCap[p]) {
          std::printf("FAIL: level %d seed %d has %d of piece %d\n", level, seed, per[p], p);
          failures++;
        }
      }
    }
    std::sort(chances.begin(), chances.end());
    std::printf("  %-12s", def.name);
    for (int n = 2; n <= kMaxPieces; n++) {
      if (sizes[n]) std::printf(" %dp:%d", n, sizes[n]);
    }
    std::printf("  random play wins: median %.1f%%, worst %.1f%%  avg %.1f ms\n", chances[chances.size() / 2] / 655.35,
                chances.back() / 655.35, total / perLevel);
  }
  std::printf("  slowest puzzle: %.1f ms on the PC, with the sanitizers\n", slowest);

  // The same seed makes the same puzzle.
  Rng a(42), b(42);
  const Board x = generate(a, 2, memo), y = generate(b, 2, memo);
  check(std::memcmp(x.cell, y.cell, kCells) == 0, "a seed repeats its puzzle");
}

// Plays the first winning line found, through the flow, as taps.
bool solveByTaps(State& s) {
  if (isWon(s)) return true;
  for (int from = 0; from < kCells; from++) {
    int t[8];
    const int n = capturesFrom(s.board, from, t);
    for (int k = 0; k < n; k++) {
      Board after = s.board;
      capture(after, from, t[k]);
      if (countWins(after, 1) == 0) continue;
      tapCell(s, from);
      tapCell(s, t[k]);
      return solveByTaps(s);
    }
  }
  return false;
}

void testFlow() {
  State s;
  Board b;
  b.cell[at(3, 0)] = Rook;
  b.cell[at(0, 0)] = Pawn;
  b.cell[at(0, 3)] = Pawn;
  b.cell[at(2, 2)] = Knight;  // no capture: (0,3) and (0,1)? (0,1) empty, (0,3) yes
  check(newPuzzle(s, b) == Change::Full, "new puzzle is a full redraw");
  check(!canUndo(s), "nothing to undo at the start");
  check(undoMove(s) == Change::None, "undo at the start does nothing");
  check(tapCell(s, at(1, 1)) == Change::None, "empty cell with nothing picked up");
  check(tapOff(s) == Change::None, "off the board with nothing picked up");

  // A pawn on the top row can't capture: says so once, then nothing.
  check(tapCell(s, at(0, 0)) == Change::View && s.message == Message::CannotCapture, "stuck piece says so");
  check(tapCell(s, at(0, 0)) == Change::None, "saying it again changes nothing");

  check(tapCell(s, at(3, 0)) == Change::View && s.sel == at(3, 0), "pick up the rook");
  check(tapCell(s, at(3, 0)) == Change::View && s.sel == kNoCell, "put it down again");
  check(tapCell(s, at(3, 0)) == Change::View, "pick it up again");
  check(tapCell(s, at(2, 2)) == Change::View && s.sel == at(2, 2), "another capturing piece switches");
  check(tapOff(s) == Change::View && s.sel == kNoCell, "tap off puts down");

  check(tapCell(s, at(3, 0)) == Change::View, "rook again");
  check(tapCell(s, at(0, 0)) == Change::Move, "rook takes the pawn");
  check(s.board.cell[at(0, 0)] == Rook && s.taken[0] == Pawn && s.moveCount == 1, "capture recorded");
  check(canUndo(s), "can undo a capture");
  check(undoMove(s) == Change::Move && s.board.cell[at(3, 0)] == Rook && s.board.cell[at(0, 0)] == Pawn, "undo puts it back");

  // Win through taps, counted once even after undo and winning again.
  check(solveByTaps(s), "the puzzle can be won by taps");
  check(isWon(s) && isOver(s) && s.solved[0] == 1 && s.counted, "win counted");
  check(tapCell(s, at(0, 0)) == Change::None, "taps after the end do nothing");
  check(undoMove(s) == Change::Full, "undo out of a win is a full redraw");
  check(solveByTaps(s) && s.solved[0] == 1, "winning again doesn't count twice");

  // A loss.
  Board lose;
  lose.cell[at(3, 0)] = Rook;
  lose.cell[at(0, 0)] = Pawn;
  lose.cell[at(1, 3)] = Pawn;
  newPuzzle(s, lose);
  check(!s.counted && s.moveCount == 0, "new puzzle resets");
  tapCell(s, at(3, 0));
  check(tapCell(s, at(0, 0)) == Change::Full && isLost(s), "taking the wrong pawn loses");
  check(s.solved[0] == 1, "a loss isn't counted");
  check(undoMove(s) == Change::Full && !isOver(s), "undo out of a loss");

  check(nextLevel(s) == 1, "level steps");
  s.level = 3;
  check(nextLevel(s) == 0, "level wraps");
}

void testSave() {
  State s;
  Rng rng(7);
  std::vector<uint64_t> slots;
  Memo memo = makeMemo(slots);
  newPuzzle(s, generate(rng, 3, memo));
  s.level = 3;
  s.solved[0] = 300;
  s.solved[3] = 2;
  solveByTaps(s);
  undoMove(s);
  undoMove(s);
  uint8_t data[SAVE_MAX];
  const size_t size = pack(s, data);
  check(size <= SAVE_MAX && size <= 512, "save fits");

  State r;
  check(unpack(SAVE_VERSION, data, size, r), "save loads");
  check(std::memcmp(r.board.cell, s.board.cell, kCells) == 0 && r.moveCount == s.moveCount, "board comes back");
  check(r.level == 3 && r.solved[0] == 300 && r.solved[3] == s.solved[3] && r.counted == s.counted, "record comes back");

  State keep = r;
  check(!unpack(SAVE_VERSION + 1, data, size, keep), "wrong version refused");
  check(!unpack(SAVE_VERSION, data, size - 1, keep), "short save refused");
  uint8_t bad[SAVE_MAX];
  std::memcpy(bad, data, size);
  bad[0] = kLevels;
  check(!unpack(SAVE_VERSION, bad, size, keep), "bad level refused");
  std::memcpy(bad, data, size);
  bad[1] = 2;
  check(!unpack(SAVE_VERSION, bad, size, keep), "bad flag refused");
  std::memcpy(bad, data, size);
  bad[10] = 0x70;
  check(!unpack(SAVE_VERSION, bad, size, keep), "bad piece code refused");
  std::memcpy(bad, data, size);
  bad[SAVE_HEADER] = static_cast<uint8_t>((bad[SAVE_HEADER] & 0xF0) | ((bad[SAVE_HEADER] >> 4) & 0x0F));
  check(!unpack(SAVE_VERSION, bad, size, keep), "a move onto itself refused");
  std::memset(bad, 0, sizeof(bad));
  check(!unpack(SAVE_VERSION, bad, SAVE_HEADER, keep), "empty board refused");
  check(keep.level == 3 && keep.moveCount == r.moveCount, "a refused save leaves the state alone");
}

}  // namespace

int main() {
  testMoves();
  testSolver();
  testWinChance();
  testGenerator();
  testFlow();
  testSave();
  if (failures) {
    std::printf("gridgambit: %d failed\n", failures);
    return 1;
  }
  std::printf("gridgambit: all passed\n");
  return 0;
}
