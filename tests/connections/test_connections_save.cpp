// Connections' save packing, checked on the PC.
#include <cstdio>
#include <cstring>

#include "connections_save.h"

namespace {

int failures = 0;

void expect(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    failures++;
  }
}

connections::Puzzle makePuzzle() {
  connections::Puzzle puzzle;
  puzzle.id = 7;
  puzzle.date = 20240315;
  for (int g = 0; g < connections::kGroups; g++) {
    snprintf(puzzle.groups[g].name, sizeof(puzzle.groups[g].name), "GROUP %d", g);
    puzzle.groups[g].level = static_cast<uint8_t>(g);
    for (int m = 0; m < connections::kMembers; m++) snprintf(puzzle.groups[g].members[m], sizeof(puzzle.groups[g].members[m]), "W%d%d", g, m);
  }
  return puzzle;
}

// Selects the four tiles of group g, wherever the shuffle put them.
void selectGroup(connections::Game& game, int group) {
  game.deselectAll();
  for (int i = 0; i < game.tileCount(); i++) {
    if (game.tileGroup(i) == group) game.toggleTile(i);
  }
}

// A game part way through: one group solved, one wrong guess, two tiles selected.
connections::OpenBoard midGame(connections::Game& game) {
  const connections::Puzzle puzzle = makePuzzle();
  game.start(puzzle, 12345);
  selectGroup(game, 2);
  game.submit();
  game.deselectAll();
  // Three of group 0 and one of group 1: one away, and a mistake.
  int picked0 = 0;
  for (int i = 0; i < game.tileCount() && picked0 < 3; i++) {
    if (game.tileGroup(i) == 0) {
      game.toggleTile(i);
      picked0++;
    }
  }
  for (int i = 0; i < game.tileCount(); i++) {
    if (game.tileGroup(i) == 1) {
      game.toggleTile(i);
      break;
    }
  }
  game.submit();
  game.deselectAll();
  // Two words of group 3, picked by word, not by where the shuffle put them.
  int picked3 = 0;
  for (int i = 0; i < game.tileCount() && picked3 < 2; i++) {
    if (game.tileGroup(i) == 3) {
      game.toggleTile(i);
      picked3++;
    }
  }

  connections::OpenBoard board;
  board.index = 300;
  board.date = puzzle.date;
  board.state = game.save();
  for (int i = 0; i < game.tileCount(); i++) {
    if (!game.isSelected(i)) continue;
    for (int m = 0; m < connections::kMembers; m++) {
      if (strcmp(puzzle.groups[3].members[m], game.tileWord(i)) == 0) board.selected = static_cast<uint16_t>(board.selected | (1u << (3 * connections::kMembers + m)));
    }
  }
  return board;
}

void testRoundTrip() {
  connections::Game game;
  const connections::OpenBoard board = midGame(game);
  expect(board.state.solvedCount == 1 && board.state.mistakes == 1 && board.state.guessCount == 2, "the mid game is what the test meant");

  uint8_t bytes[connections::SAVE_BYTES];
  expect(connections::packBoard(board, bytes) == connections::SAVE_BYTES, "packs to SAVE_BYTES");

  connections::OpenBoard back;
  expect(connections::unpackBoard(connections::SAVE_VERSION, bytes, sizeof(bytes), back), "round trip unpacks");
  expect(back.index == 300 && back.date == 20240315 && back.selected == board.selected, "index, date and selection survive");
  expect(board.selected != 0 && (board.selected & 0x0FFF) == 0, "the selection is two words of group 3");
  expect(back.state.seed == board.state.seed && back.state.solvedOrder[0] == 2 && back.state.mistakes == 1, "seed, solved group and mistakes survive");
  expect(memcmp(back.state.guessed, board.state.guessed, sizeof(board.state.guessed)) == 0, "guesses survive");

  connections::Game restored;
  restored.start(makePuzzle(), 0);
  expect(restored.restore(back.state), "Game::restore accepts it");
  expect(restored.solvedCount() == 1 && restored.mistakes() == 1, "the restored game has the progress");
  // Submitting the group already solved is impossible, since its tiles are gone, and
  // the wrong guess came back in the history.
  expect(restored.save().guessCount == 2, "the guess history came back");
  expect(restored.tileCount() == 12, "the solved group left the board");
}

void testRejects() {
  connections::Game game;
  const connections::OpenBoard board = midGame(game);
  uint8_t good[connections::SAVE_BYTES];
  connections::packBoard(board, good);
  connections::OpenBoard out;
  out.index = -5;

  expect(!connections::unpackBoard(connections::SAVE_VERSION + 1, good, sizeof(good), out), "rejects another version");
  expect(!connections::unpackBoard(connections::SAVE_VERSION, good, sizeof(good) - 1, out), "rejects a short save");

  struct Damage {
    int at;
    uint8_t value;
    const char* what;
  };
  const Damage damages[] = {
      {5, 0x00, "rejects a date that isn't one"},
      {10, 4, "rejects four groups solved"},
      {11, 7, "rejects a solved group out of range"},
      {12, 2, "rejects a solved group listed after the count"},
      {15, 4, "rejects four mistakes"},
      {16, 9, "rejects more than eight guesses"},
      {17, 0xFF, "rejects a guess that isn't four words"},
      {21, 0x0F, "rejects a guess past the count"},
      {33, 0xFF, "rejects more than four words selected"},
      {34, 0x01, "rejects a selected word from a solved group"},
  };
  for (const Damage& damage : damages) {
    uint8_t bad[connections::SAVE_BYTES];
    memcpy(bad, good, sizeof(bad));
    bad[damage.at] = damage.value;
    expect(!connections::unpackBoard(connections::SAVE_VERSION, bad, sizeof(bad), out), damage.what);
  }
  expect(out.index == -5, "a refused save changes nothing");
}

}  // namespace

int main() {
  testRoundTrip();
  testRejects();
  if (failures == 0) std::printf("Connections save: all checks passed\n");
  return failures == 0 ? 0 : 1;
}
