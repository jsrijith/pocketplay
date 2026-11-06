// D&Diagrams' save packing, checked on the PC.
#include <cstdio>
#include <cstring>

#include "dungeon_save.h"

namespace {

int failures = 0;

void expect(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    failures++;
  }
}

// The first cell of dungeon `index` that holds neither a monster nor a chest.
void freeCell(int index, int& row, int& col) {
  dungeon::Board board;
  board.load(index);
  for (row = 0; row < board.size(); row++) {
    for (col = 0; col < board.size(); col++) {
      if (!board.isMonster(row, col) && !board.isChest(row, col)) return;
    }
  }
}

void testRoundTrip() {
  dungeon::Board board;
  board.load(37);
  int row = 0;
  int col = 0;
  freeCell(37, row, col);
  board.tap(row, col);      // wall
  board.tap(row, col + 1);  // wall on the next cell, if it's free
  dungeon::Progress progress;
  progress.markSolved(1);
  progress.markSolved(63);
  progress.markSolved(64);

  uint8_t bytes[dungeon::SAVE_BYTES];
  expect(dungeon::packGame(board, progress, bytes) == dungeon::SAVE_BYTES, "packs to its size");

  dungeon::Board back;
  dungeon::Progress record;
  expect(dungeon::unpackGame(dungeon::SAVE_VERSION, bytes, sizeof(bytes), back, record), "unpacks its own bytes");
  expect(back.index() == 37, "keeps the dungeon");
  expect(back.wallMask() == board.wallMask() && back.floorMask() == board.floorMask(), "keeps the marks");
  expect(record.low == progress.low && record.high == progress.high, "keeps the record");
  expect(record.solvedCount() == 3, "counts the record");
}

void testRejects() {
  dungeon::Board board;
  board.load(5);
  dungeon::Progress progress;
  uint8_t good[dungeon::SAVE_BYTES];
  dungeon::packGame(board, progress, good);

  dungeon::Board out;
  out.load(9);
  dungeon::Progress record;
  record.low = 4;

  expect(!dungeon::unpackGame(dungeon::SAVE_VERSION + 1, good, sizeof(good), out, record), "refuses another version");
  expect(!dungeon::unpackGame(dungeon::SAVE_VERSION, good, sizeof(good) - 1, out, record), "refuses a short file");

  uint8_t bad[dungeon::SAVE_BYTES];
  memcpy(bad, good, sizeof(bad));
  bad[0] = 0;
  expect(!dungeon::unpackGame(dungeon::SAVE_VERSION, bad, sizeof(bad), out, record), "refuses the tutorial");
  bad[0] = dungeon::kPuzzleCount;
  expect(!dungeon::unpackGame(dungeon::SAVE_VERSION, bad, sizeof(bad), out, record), "refuses a dungeon past the end");

  memcpy(bad, good, sizeof(bad));
  bad[17] = 1;  // the tutorial marked solved
  expect(!dungeon::unpackGame(dungeon::SAVE_VERSION, bad, sizeof(bad), out, record), "refuses a solved tutorial");
  memcpy(bad, good, sizeof(bad));
  bad[25] = 2;  // dungeon 65, which doesn't exist
  expect(!dungeon::unpackGame(dungeon::SAVE_VERSION, bad, sizeof(bad), out, record), "refuses a dungeon past the end in the record");

  expect(out.index() == 9 && record.low == 4, "a refused save changes nothing");
}

void testMarksOnFurnitureDropped() {
  // Find a dungeon with a monster, and put a wall on it in the bytes.
  for (int index = dungeon::kCampaignFirst; index < dungeon::kPuzzleCount; index++) {
    const uint64_t monsters = dungeon::kPuzzles[index].monsters;
    if (monsters == 0) continue;
    dungeon::Board board;
    board.load(index);
    dungeon::Progress progress;
    uint8_t bytes[dungeon::SAVE_BYTES];
    dungeon::packGame(board, progress, bytes);
    for (int i = 0; i < 8; i++) bytes[1 + i] = static_cast<uint8_t>(monsters >> (8 * i));

    dungeon::Board back;
    expect(dungeon::unpackGame(dungeon::SAVE_VERSION, bytes, sizeof(bytes), back, progress), "accepts marks on a monster");
    expect(back.wallMask() == 0, "but drops them");
    return;
  }
  expect(false, "some dungeon has a monster");
}

}  // namespace

int main() {
  testRoundTrip();
  testRejects();
  testMarksOnFurnitureDropped();
  if (failures != 0) return 1;
  std::printf("dungeon save: all passed\n");
  return 0;
}
