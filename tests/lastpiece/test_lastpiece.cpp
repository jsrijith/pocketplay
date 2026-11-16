// Last Piece's rules, card files, flow and save: how each piece captures, the solver and its
// hints, reading boards and solutions in every form the README allows and rejecting the rest,
// whole challenges won and lost, PREV and NEXT, a card edited under a saved position, and save
// and reload. Every board here is made up for the test.
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "lastpiece_flow.h"
#include "lastpiece_pack.h"
#include "lastpiece_rules.h"
#include "lastpiece_save.h"

using namespace lastpiece;

namespace {

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    failures++;
  }
}

int at(int r, int c) { return r * kSide + c; }

std::vector<uint64_t> slots(1024);
Memo memo{slots.data(), 1023};

// A Pack is 3 KB, so the tests keep theirs out of the stack.
Pack& parse(const std::string& boards, const std::string& solutions = "") {
  static Pack pack;
  parsePack(reinterpret_cast<const uint8_t*>(boards.data()), boards.size(), pack, memo);
  if (!solutions.empty()) parseSolutions(reinterpret_cast<const uint8_t*>(solutions.data()), solutions.size(), pack);
  return pack;
}

// Two made-up challenges: a queen, knight, pawn and rook; then a bishop, king, knight, pawn and rook.
const char* const kTwo =
    "# two challenges\n"
    "Q . . .\n"
    ". . N .\n"
    ". P . .\n"
    ". . . R\n"
    "\n"
    ".B..\n"
    "K..N\n"
    "..P.\n"
    "R...\n";
const char* const kTwoSolutions =
    "01: 0,0->3,3 1,2->3,3 3,3->2,1\n"
    "02: 0,1->1,0 2,2->1,3 3,0->1,0 1,0->1,3\n";

void testMoves() {
  Board b;
  b.cell[at(3, 0)] = Rook;
  b.cell[at(1, 0)] = Pawn;
  b.cell[at(0, 0)] = Knight;
  b.cell[at(3, 3)] = Bishop;
  check(canCapture(b, at(3, 0), at(1, 0)), "rook takes up its column");
  check(!canCapture(b, at(3, 0), at(0, 0)), "rook is blocked");
  check(canCapture(b, at(3, 0), at(3, 3)), "rook takes along its row");

  // The pawn takes diagonally towards row 0, never down or straight.
  b = Board{};
  b.cell[at(2, 1)] = Pawn;
  b.cell[at(1, 0)] = Rook;
  b.cell[at(1, 1)] = Rook;
  b.cell[at(3, 2)] = Rook;
  check(canCapture(b, at(2, 1), at(1, 0)), "pawn takes up and left");
  check(!canCapture(b, at(2, 1), at(1, 1)), "pawn doesn't take straight ahead");
  check(!canCapture(b, at(2, 1), at(3, 2)), "pawn doesn't take downwards");

  // The knight jumps over what's between.
  b = Board{};
  b.cell[at(3, 0)] = Knight;
  b.cell[at(2, 0)] = Pawn;
  b.cell[at(2, 1)] = Pawn;
  b.cell[at(1, 1)] = Rook;
  check(canCapture(b, at(3, 0), at(1, 1)), "knight jumps");
  check(!canCapture(b, at(3, 0), at(2, 1)), "knight doesn't take next to it");

  check(capture(b, at(3, 0), at(0, 0)) == None, "a move to an empty square isn't a capture");
  check(b.cell[at(3, 0)] == Knight, "a refused move leaves the board alone");
}

void testSolver() {
  Board b;
  b.cell[at(0, 0)] = Rook;
  b.cell[at(0, 3)] = Bishop;
  check(solvable(b, memo), "two pieces that see each other can be won");
  b.cell[at(0, 3)] = None;
  b.cell[at(1, 2)] = Bishop;
  check(!solvable(b, memo), "two pieces that can't take can't be won");
  // The answer is the same from the cache.
  check(!solvable(b, memo), "the cached answer is the same");
  Memo none;
  check(!solvable(b, none), "the solver works without a cache");

  // bestMove picks a capture that still wins.
  b = Board{};
  b.cell[at(0, 0)] = Queen;
  b.cell[at(1, 2)] = Knight;
  b.cell[at(2, 1)] = Pawn;
  b.cell[at(3, 3)] = Rook;
  int from = kNoCell, to = kNoCell;
  check(bestMove(b, memo, from, to), "a winnable board has a best move");
  Board after = b;
  check(capture(after, from, to) != None && solvable(after, memo), "the best move still wins");
  // A knight beside a bishop that sees nothing: no capture at all.
  b = Board{};
  b.cell[at(0, 0)] = Knight;
  b.cell[at(0, 1)] = Bishop;
  check(!bestMove(b, memo, from, to), "a lost board has no best move");
}

void testParse() {
  Pack& p = parse(kTwo, kTwoSolutions);
  check(p.count == 2 && p.skipped == 0, "two challenges read");
  check(p.challenges[0].board.cell[at(0, 0)] == Queen && p.challenges[0].board.cell[at(3, 3)] == Rook, "first board read top row first");
  check(p.challenges[1].board.cell[at(1, 3)] == Knight, "a row without spaces reads");
  check(p.challenges[0].solutionLength == 3 && p.challenges[1].solutionLength == 4 && p.badSolutions == 0, "solutions attached");

  // CRLF, lower case, tabs, extra blank lines and comments between rows.
  Pack& q = parse("\r\n\r\nq\t.\t.\t.\r\n. . n .\r\n# a comment\r\n. p . .\r\n. . . r\r\n\r\n\r\n");
  check(q.count == 1 && q.challenges[0].board.cell[at(0, 0)] == Queen, "CRLF, lower case and tabs read");

  // A bad cell, a short row, five rows, one piece, and no win are each skipped, and the
  // numbering still counts them, so a solution after them finds its challenge.
  const std::string bad =
      "Q . X .\n. . . .\n. . . .\nR . . .\n\n"   // 1: bad cell
      "Q . .\n. . . .\n. . . .\nR . . .\n\n"     // 2: short row
      "Q...\n....\n....\nR...\n....\n\n"         // 3: five rows
      "Q...\n....\n....\n....\n\n"               // 4: one piece
      "R...\n....\n.B..\n....\n\n"               // 5: nothing can take: no win
      "R...\n....\n....\nB...\n";                // 6: good
  Pack& r = parse(bad, "06: 0,0->3,0\n");
  check(r.count == 1 && r.skipped == 5, "bad challenges skipped");
  check(r.challenges[0].number == 6 && r.challenges[0].solutionLength == 1, "numbering counts the skipped ones");

  // Eleven pieces is too many.
  Pack& s = parse("QRRB\nBNNP\nPK..\n.P..\n");
  check(s.count == 0 && s.skipped == 1, "more than ten pieces is skipped");

  // A file with 100 good challenges keeps 99.
  std::string many;
  for (int k = 0; k < 100; k++) many += "R...\n....\n....\nB...\n\n";
  Pack& m = parse(many);
  check(m.count == kMaxChallenges && m.overLimit == 1, "the 100th challenge is left out");

  // Solutions: spaces around the arrow, out of order, a wrong move, one that doesn't finish, an
  // unknown number, a second one for the same challenge, and a line that doesn't read.
  Pack& t = parse(kTwo,
                  "02: 0,1 -> 1,0  2,2->1,3 3,0->1,0 1,0->1,3\n"
                  "01: 0,0->3,3 1,2->3,3 3,3->2,1\n"
                  "01: 0,0->3,3 1,2->3,3 3,3->2,1\n");
  check(t.challenges[0].solutionLength == 3 && t.challenges[1].solutionLength == 4, "spaces and order don't matter");
  check(t.badSolutions == 1, "a second solution for one challenge is dropped");
  Pack& u = parse(kTwo,
                  "01: 0,0->0,1 1,2->3,3 3,3->2,1\n"   // not a capture
                  "02: 0,1->1,0\n"                    // doesn't end on one piece
                  "03: 0,0->3,3\n"                    // no challenge 3
                  "x1: 0,0->3,3\n"                    // not a number
                  "01 0,0->3,3 1,2->3,3 3,3->2,1\n"   // no colon
                  "02: 0,1->1,0 2,2->1,4\n");         // off the board
  check(u.badSolutions == 6 && u.challenges[0].solutionLength == 0 && u.challenges[1].solutionLength == 0,
        "bad solutions dropped");
}

void testPlay() {
  Pack& p = parse(kTwo, kTwoSolutions);
  State s;
  check(sync(s, p), "a fresh state takes the first challenge");
  check(startPieces(s) == 4 && !isOver(s), "first challenge starts with four pieces");

  // Tapping a piece that can't take says so; tapping an empty square does nothing.
  check(tapCell(s, at(0, 1)) == Change::None, "an empty square with nothing picked up does nothing");
  check(tapCell(s, at(3, 3)) == Change::View && s.message == Message::CannotCapture, "the rook can't take");
  check(tapCell(s, at(3, 3)) == Change::None, "saying it again changes nothing");

  // HINT follows the solution, and works off it with the solver.
  check(hint(s, p, memo) == Change::View && s.sel == at(0, 0) && s.hintTo == at(3, 3), "hint gives the solution's first move");
  check(hint(s, p, memo) == Change::None, "a second hint changes nothing");
  check(tapCell(s, at(3, 3)) == Change::Move && s.moveCount == 1 && s.hintTo == kNoCell, "taking the hinted piece");
  check(hint(s, p, memo) == Change::View && s.sel == at(1, 2) && s.hintTo == at(3, 3), "hint gives the next move");
  check(tapOff(s) == Change::View && s.sel == kNoCell && s.hintTo == kNoCell, "tapping off drops the hint");
  check(tapOff(s) == Change::None, "tapping off again changes nothing");

  check(undoMove(s) == Change::Move && s.moveCount == 0, "undo back to the start");

  // Play the first challenge out and check it's marked solved once.
  check(tapCell(s, at(0, 0)) == Change::View, "pick up the queen");
  check(tapCell(s, at(3, 3)) == Change::Move, "queen takes rook");
  check(tapCell(s, at(1, 2)) == Change::View, "pick up the knight");
  check(tapCell(s, at(3, 3)) == Change::Move, "knight takes queen");
  check(tapCell(s, at(3, 3)) == Change::View, "pick up the knight again");
  check(tapCell(s, at(2, 1)) == Change::Full && isWon(s), "knight takes pawn: solved");
  check(isSolved(s, 0, 0) && solvedCount(s, 0, p.count) == 1, "solved mark set");
  check(tapCell(s, at(2, 1)) == Change::None, "nothing more after the end");
  check(hint(s, p, memo) == Change::None, "no hint after the end");
  check(undoMove(s) == Change::Full && !isOver(s), "undo out of the end is full");
  check(tapCell(s, at(3, 3)) == Change::View && tapCell(s, at(2, 1)) == Change::Full, "solve again");
  check(solvedCount(s, 0, p.count) == 1, "a second solve doesn't count twice");

  // RESET puts the pieces back, keeps the solved mark, and does nothing at the start.
  check(resetChallenge(s) == Change::Full && s.moveCount == 0 && startPieces(s) == 4 && !isOver(s), "reset");
  check(sameBoard(s.board, p.challenges[0].board), "reset restores the starting board");
  check(isSolved(s, 0, 0), "reset keeps the solved mark");
  check(resetChallenge(s) == Change::None, "reset at the start does nothing");

  // A lost position: the hint says there's no win.
  State l;
  sync(l, p);
  check(tapCell(l, at(2, 1)) == Change::View, "pick up the pawn");
  check(tapCell(l, at(1, 2)) == Change::Move, "pawn takes knight");
  check(hint(l, p, memo) == Change::View && l.message == Message::NoWin && l.sel == kNoCell, "no win from here");

  // PREV and NEXT wrap, and start the challenge afresh.
  check(step(s, p, 1) == Change::Full && s.challenge[0] == 1 && s.moveCount == 0, "next");
  check(step(s, p, 1) == Change::Full && s.challenge[0] == 0, "next wraps");
  check(step(s, p, -1) == Change::Full && s.challenge[0] == 1, "prev wraps");
  check(startPieces(s) == 5, "second challenge has five pieces");

  // With one challenge, PREV and NEXT do nothing.
  Pack& one = parse("R...\n....\n....\nB...\n");
  State o;
  sync(o, one);
  check(step(o, one, 1) == Change::None, "next with one challenge does nothing");
}

void testDifficulty() {
  static Pack beginner, expert;
  beginner = parse(kTwo);
  expert = parse("R...\n....\n....\nB...\n");
  static Pack empty;
  empty.count = 0;
  State s;
  sync(s, beginner);
  step(s, beginner, 1);
  tapCell(s, at(0, 1));
  tapCell(s, at(1, 0));
  check(s.moveCount == 1, "a move on challenge 2");
  check(chooseDifficulty(s, empty, 1) == Change::None && s.difficulty == 0, "an empty difficulty can't be chosen");
  check(chooseDifficulty(s, beginner, 0) == Change::Full && s.moveCount == 1, "choosing the same difficulty keeps the moves");
  check(chooseDifficulty(s, expert, 3) == Change::Full && s.difficulty == 3 && s.moveCount == 0, "another difficulty starts its challenge");
  check(s.challenge[0] == 1, "each difficulty keeps its place");
  check(chooseDifficulty(s, beginner, 0) == Change::Full && s.challenge[0] == 1 && startPieces(s) == 5, "back to where you were");
}

void testSync() {
  static Pack before, after;
  before = parse(kTwo);
  State s;
  sync(s, before);
  tapCell(s, at(0, 0));
  tapCell(s, at(3, 3));
  check(!sync(s, before) && s.moveCount == 1, "the same card keeps the moves");
  // The first challenge is edited on the card: the moves are dropped.
  after = parse("R...\n....\n....\nB...\n\n.B..\nK..N\n..P.\nR...\n");
  check(sync(s, after) && s.moveCount == 0 && startPieces(s) == 2, "an edited challenge starts again");
  // The card loses challenges: the place falls back to the first.
  s.challenge[0] = 1;
  sync(s, before);
  static Pack shorter;
  shorter = parse("R...\n....\n....\nB...\n");
  check(sync(s, shorter) && s.challenge[0] == 0, "a place past the end goes back to the first");
  // No challenges at all leaves an empty board.
  static Pack none;
  none.count = 0;
  check(sync(s, none) && startPieces(s) == 0 && !isOver(s), "no challenges, empty board");
}

void testSave() {
  Pack& p = parse(kTwo);
  State s;
  sync(s, p);
  s.difficulty = 0;
  s.challenge[2] = 7;
  s.solved[0][0] = 0x05;
  s.solved[3][kSolvedBytes - 1] = 0x01;
  tapCell(s, at(0, 0));
  tapCell(s, at(3, 3));
  uint8_t data[SAVE_MAX];
  const size_t size = pack(s, data);
  check(size == SAVE_HEADER + 1, "save size");
  State r;
  check(unpack(SAVE_VERSION, data, size, r), "save reloads");
  check(r.moveCount == 1 && sameBoard(r.board, s.board) && r.challenge[2] == 7, "position and places kept");
  check(r.solved[0][0] == 0x05 && r.solved[3][kSolvedBytes - 1] == 0x01, "solved marks kept");
  check(!sync(r, p) && r.moveCount == 1, "the reloaded game matches the card");

  check(!unpack(SAVE_VERSION + 1, data, size, r), "wrong version rejected");
  check(!unpack(SAVE_VERSION, data, size - 1, r), "short save rejected");
  uint8_t bad[SAVE_MAX];
  std::memcpy(bad, data, size);
  bad[0] = kDifficulties;
  check(!unpack(SAVE_VERSION, bad, size, r), "bad difficulty rejected");
  std::memcpy(bad, data, size);
  bad[1] = kMaxChallenges;
  check(!unpack(SAVE_VERSION, bad, size, r), "challenge past 99 rejected");
  std::memcpy(bad, data, size);
  bad[1 + kDifficulties + kSolvedBytes - 1] = 0x10;
  check(!unpack(SAVE_VERSION, bad, size, r), "a solved bit past 99 rejected");
  std::memcpy(bad, data, size);
  bad[1 + kDifficulties + kDifficulties * kSolvedBytes] = 0x70;
  check(!unpack(SAVE_VERSION, bad, size, r), "bad piece code rejected");
  std::memcpy(bad, data, size);
  bad[size - 1] = static_cast<uint8_t>(at(0, 1) << 4 | at(0, 2));
  check(!unpack(SAVE_VERSION, bad, size, r), "a move that isn't a capture rejected");

  // An empty board with no moves is a card with nothing on it yet.
  State e;
  const size_t emptySize = pack(e, data);
  check(unpack(SAVE_VERSION, data, emptySize, r) && startPieces(r) == 0, "an empty save reloads");
}

}  // namespace

int main() {
  testMoves();
  testSolver();
  testParse();
  testPlay();
  testDifficulty();
  testSync();
  testSave();
  if (failures) {
    std::printf("%d failure(s)\n", failures);
    return 1;
  }
  std::printf("Last Piece: all tests passed\n");
  return 0;
}
