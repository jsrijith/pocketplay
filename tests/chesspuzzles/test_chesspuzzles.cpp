// Chess Puzzles' rules, flow and save, checked the way touch-licorice-apps' test_chess.py checks
// the Lua: the first puzzle, a wrong move, HINT twice, SHOW, NEXT, the banks, resuming a puzzle
// half way; castling, en passant and promotion; and every puzzle of every bank solved by taps.
// Squares here are as the rules number them (a1 = 0); the screen turns the board for black.
#include <cstdio>
#include <cstring>

#include "chesspuzzles_flow.h"
#include "chesspuzzles_rules.h"
#include "chesspuzzles_save.h"

using namespace chesspuzzles;

namespace {

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    failures++;
  }
}

int sq(const char* name) { return squareOf(name); }

// Plays the solution as taps: the piece, then its square.
bool solveByTaps(State& s) {
  int guard = 0;
  while (!s.done && guard++ < kMaxMoves) {
    const Move& m = s.puzzle.moves[s.ply];
    if (tapSquare(s, m.from) != Change::View) return false;
    tapSquare(s, m.to);
  }
  return s.done;
}

bool sameBoard(const Board& a, const Board& b) { return std::memcmp(a.sq, b.sq, kSquares) == 0; }

void testFirstPuzzle() {
  State s;
  check(start(s, 2, 0), "first Easy puzzle reads");
  check(s.bank == 2 && s.ply == 1, "opponent's move made");
  Board fen;
  setFen(fen, s.puzzle.fen);
  check(s.me == (fen.turn == 'w' ? 'b' : 'w'), "the player is the side after the opponent's move");
  check(s.board.lastFrom == s.puzzle.moves[0].from && s.board.lastTo == s.puzzle.moves[0].to, "opponent's move marked");

  // Nothing picked up: an empty square or an opponent's piece does nothing.
  int theirs = kNoSquare, empty = kNoSquare;
  for (int i = 0; i < kSquares; i++) {
    if (s.board.sq[i] && !mine(s, s.board.sq[i]) && theirs == kNoSquare) theirs = i;
    if (!s.board.sq[i] && empty == kNoSquare) empty = i;
  }
  check(tapSquare(s, empty) == Change::None && tapSquare(s, theirs) == Change::None, "taps with nothing picked up");
  check(tapOff(s) == Change::None, "tap off with nothing picked up");

  // A wrong move: told, counted, nothing moves.
  const Board before = s.board;
  const Move want = s.puzzle.moves[1];
  int wrong = kNoSquare;
  for (int i = 0; i < kSquares && wrong == kNoSquare; i++) {
    if (i != want.to && !mine(s, s.board.sq[i])) wrong = i;
  }
  check(tapSquare(s, want.from) == Change::View && s.sel == want.from, "pick up the piece");
  check(tapSquare(s, want.from) == Change::View && s.sel == kNoSquare, "put it down");
  tapSquare(s, want.from);
  check(tapSquare(s, wrong) == Change::Move, "wrong move counted");
  check(sameBoard(before, s.board) && s.message == Message::NotTheMove && s.mistakes == 1, "wrong move told, board kept");
  check(s.sel == kNoSquare, "wrong move puts the piece down");

  // HINT twice, then nothing more; SHOW plays the move.
  check(showHint(s) == Change::View && s.hint == 1 && s.message == Message::HintPiece, "first hint");
  check(showHint(s) == Change::View && s.hint == 2 && s.message == Message::HintSquare, "second hint");
  check(showHint(s) == Change::None, "a third hint changes nothing");
  const Change c = showMove(s);
  check(s.shown && s.hint == 0 && (c == Change::Move || c == Change::Full), "show plays the move");
  check(solveByTaps(s) && s.message == Message::Solved, "the rest by taps");
  check(s.solved[2] == 1 && s.clean[2] == 0, "a shown puzzle isn't first try");
  check(showHint(s) == Change::None && showMove(s) == Change::None && tapSquare(s, want.to) == Change::None, "nothing after the end");

  // NEXT; a puzzle solved first try is counted clean.
  check(next(s) == Change::Full && s.index[2] == 1 && !s.done, "next");
  check(solveByTaps(s) && s.mistakes == 0, "solved first try");
  check(s.solved[2] == 2 && s.clean[2] == 1, "clean counted");
  check(previous(s) == Change::Full && s.index[2] == 0, "previous");
  check(previous(s) == Change::None, "previous stops at the first");
  start(s, 2, kBankList[2].count - 1);
  check(next(s) == Change::Full && s.index[2] == 0, "next wraps round");
}

void testResumeAndSave() {
  State s;
  start(s, 2, 5);
  s.solved[0] = 7;
  s.clean[0] = 3;
  check(pickBank(s, 1) == Change::Full && s.bank == 1, "pick Mate in 2");
  const Move m = s.puzzle.moves[1];
  tapSquare(s, m.from);
  check(tapSquare(s, m.to) == Change::Move && s.ply == 3 && s.message == Message::Good, "first move of a mate in 2");
  showHint(s);
  uint8_t data[SAVE_SIZE];
  check(pack(s, data) == SAVE_SIZE, "save size");

  State r;
  check(unpack(SAVE_VERSION, data, SAVE_SIZE, r), "save loads");
  check(r.bank == 1 && r.ply == 3 && !r.done && sameBoard(r.board, s.board), "half-way puzzle resumed");
  check(r.index[2] == 5 && r.solved[0] == 7 && r.clean[0] == 3, "banks resumed");
  check(solveByTaps(r) && r.done, "and finished");

  pack(r, data);
  State d;
  check(unpack(SAVE_VERSION, data, SAVE_SIZE, d) && d.done && d.message == Message::Solved && d.solved[1] == 1, "a finished puzzle stays finished");

  State keep = d;
  uint8_t bad[SAVE_SIZE];
  check(!unpack(SAVE_VERSION + 1, data, SAVE_SIZE, keep), "wrong version refused");
  check(!unpack(SAVE_VERSION, data, SAVE_SIZE - 1, keep), "short save refused");
  std::memcpy(bad, data, SAVE_SIZE);
  bad[0] = kBanks;
  check(!unpack(SAVE_VERSION, bad, SAVE_SIZE, keep), "bad bank refused");
  std::memcpy(bad, data, SAVE_SIZE);
  bad[1] = 0xFF;
  check(!unpack(SAVE_VERSION, bad, SAVE_SIZE, keep), "puzzle past the bank refused");
  std::memcpy(bad, data, SAVE_SIZE);
  bad[6] = 0xFF;  // bank 0's clean count over its solved
  check(!unpack(SAVE_VERSION, bad, SAVE_SIZE, keep), "clean over solved refused");
  std::memcpy(bad, data, SAVE_SIZE);
  bad[SAVE_SIZE - 1] = 0;  // finished flag off with the ply at the end
  check(!unpack(SAVE_VERSION, bad, SAVE_SIZE, keep), "ply at the end of an open puzzle refused");
  std::memcpy(bad, data, SAVE_SIZE);
  bad[SAVE_SIZE - 4] = 2;
  bad[SAVE_SIZE - 1] = 0;
  check(!unpack(SAVE_VERSION, bad, SAVE_SIZE, keep), "an even ply refused");
  check(keep.done && keep.bank == 1, "a refused save leaves the state alone");
}

void testSpecialMoves() {
  Board b;
  Move m;
  setFen(b, "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
  parseMove("e1g1", 4, m);
  applyMove(b, m);
  parseMove("e8c8", 4, m);
  applyMove(b, m);
  check(b.sq[sq("g1")] == 'K' && b.sq[sq("f1")] == 'R' && !b.sq[sq("h1")], "short castling moves the rook");
  check(b.sq[sq("c8")] == 'k' && b.sq[sq("d8")] == 'r' && !b.sq[sq("a8")], "long castling moves the rook");
  check(b.turn == 'w', "turns alternate");

  setFen(b, "8/8/8/3pP3/8/8/8/k6K w - d6 0 1");
  parseMove("e5d6", 4, m);
  applyMove(b, m);
  check(b.sq[sq("d6")] == 'P' && !b.sq[sq("d5")] && !b.sq[sq("e5")], "en passant takes the pawn beside");

  setFen(b, "8/4P3/8/8/8/8/8/k6K w - - 0 1");
  check(parseMove("e7e8n", 5, m), "promotion parses");
  applyMove(b, m);
  check(b.sq[sq("e8")] == 'N', "promotion becomes its piece");
  setFen(b, "8/8/8/8/8/8/4p3/k6K b - - 0 1");
  parseMove("e2e1q", 5, m);
  applyMove(b, m);
  check(b.sq[sq("e1")] == 'q', "black promotes to a black piece");

  check(!setFen(b, "8/8/8/9/8/8/8/8 w - - 0 1") && !setFen(b, "8/8/8 x"), "bad FEN refused");
  check(!parseMove("e7e9", 4, m) && !parseMove("e7e8k", 5, m), "bad moves refused");
  Puzzle p;
  check(!parsePuzzle("800|8/8/8/8/8/8/8/8 w - - 0 1|e2e4 e7e5|", p), "a move from an empty square refused");

  char words[96];
  themeWords("discoveredAttack mateIn2 xRayAttack", 35, words, sizeof(words));
  check(std::strcmp(words, "DISCOVERED ATTACK, MATE IN 2, X RAY ATTACK") == 0, "themes as words");
  themeWords("", 0, words, sizeof(words));
  check(words[0] == '\0', "no themes");
}

// Every puzzle of every bank: reads, has an even number of moves, and solves by taps.
void testEveryPuzzle() {
  int total = 0;
  for (int k = 0; k < kBanks; k++) {
    for (int i = 0; i < kBankList[k].count; i++) {
      State s;
      if (!start(s, k, i)) {
        std::printf("FAIL: %s %d doesn't read\n", kBankList[k].name, i + 1);
        failures++;
        continue;
      }
      if (s.puzzle.moveCount % 2 != 0) {
        std::printf("FAIL: %s %d has an odd number of moves\n", kBankList[k].name, i + 1);
        failures++;
      }
      // The player's moves are all their own pieces, and the replies the opponent's.
      if (!solveByTaps(s) || s.clean[k] != 1) {
        std::printf("FAIL: %s %d doesn't solve by taps\n", kBankList[k].name, i + 1);
        failures++;
      }
      total++;
    }
  }
  std::printf("  %d puzzles solved by taps\n", total);
  check(total >= 2390, "every bank is there");
}

}  // namespace

int main() {
  testFirstPuzzle();
  testResumeAndSave();
  testSpecialMoves();
  testEveryPuzzle();
  if (failures) {
    std::printf("chesspuzzles: %d failed\n", failures);
    return 1;
  }
  std::printf("chesspuzzles: all passed\n");
  return 0;
}
