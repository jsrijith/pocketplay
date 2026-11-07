// Sokoban's rules, flow and saves, checked the way touch-licorice-apps' test_sokoban.py checks
// the Lua: every level is sound, moves, walls, pushes and undo, a BFS-found solution played
// through the flow ends solved with the best recorded, the buttons, and save and reload.
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "sokoban_flow.h"
#include "sokoban_rules.h"
#include "sokoban_save.h"

using namespace sokoban;

namespace {

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    failures++;
  }
}

// The state holds a long history, so tests keep it off the stack.
std::unique_ptr<State> fresh() {
  std::unique_ptr<State> s(new State());
  loadLevel(*s);
  return s;
}

// Every level: one player, as many goals as boxes, not already solved, and walled in, so
// nothing the player can reach is on the edge of the board.
void testEveryLevel() {
  for (int p = 0; p < kPackCount; p++) {
    for (int k = 0; k < kPacks[p].count; k++) {
      const LevelDef& def = kPacks[p].levels[k];
      char what[64];
      std::snprintf(what, sizeof(what), "%s %s", kPacks[p].label, def.title);
      int players = 0, boxes = 0, goals = 0;
      for (int i = 0; i < def.rows * def.cols; i++) {
        const char c = def.cells[i];
        players += (c == '@' || c == '+');
        boxes += (c == '$' || c == '*');
        goals += (c == '.' || c == '*' || c == '+');
      }
      Board b;
      build(def, b);
      bool sound = players == 1 && boxes == goals && boxes > 0 && !solved(b) && def.rows <= kMaxRows && def.cols <= kMaxCols;
      std::vector<bool> seen(b.rows * b.cols, false);
      std::vector<int> todo{b.player};
      seen[b.player] = true;
      while (!todo.empty() && sound) {
        const int i = todo.back();
        todo.pop_back();
        for (Dir d : {Dir::Left, Dir::Right, Dir::Up, Dir::Down}) {
          const int n = neighbour(b, i, d);
          if (n < 0) {
            sound = false;
            break;
          }
          if (b.cells[n] != '#' && !seen[n]) {
            seen[n] = true;
            todo.push_back(n);
          }
        }
      }
      check(sound, what);
    }
  }
  check(kTotalLevels == 385, "385 levels in all");
}

LevelDef makeLevel(const char* title, int rows, int cols, const char* cells) { return LevelDef{title, uint8_t(rows), uint8_t(cols), cells}; }

void testSteps() {
  // #######
  // #@$ $.#
  // #  #  #
  // #######
  const LevelDef def = makeLevel("T", 4, 7, "########@$ $.##  #  ########");
  Board b;
  build(def, b);
  check(step(b, Dir::Up) == Step::Blocked, "a wall stops the player");
  check(step(b, Dir::Right) == Step::Pushed && b.player == 9 && b.cells[10] == '$', "a box is pushed");
  check(step(b, Dir::Right) == Step::Blocked, "a box can't push a box");
  unstep(b, Dir::Right, true);
  check(b.player == 8 && b.cells[9] == '$' && b.cells[10] == ' ', "undoing a push brings the box back");

  // ######
  // #@$ .#
  // ######
  const LevelDef line = makeLevel("L", 3, 6, "#######@$ .#######");
  std::unique_ptr<Game> g(new Game());
  start(line, *g);
  check(move(*g, Dir::Right) && g->moves == 1 && g->pushes == 1, "a push counts as a move and a push");
  check(!move(*g, Dir::Up) && g->moves == 1, "a blocked move isn't counted");
  check(move(*g, Dir::Right) && g->board.cells[10] == '*', "onto the goal it shows as a box on a goal");
  check(solved(g->board) && !move(*g, Dir::Left), "every box on a goal is solved, and no more moves");
  check(undo(*g) && g->board.cells[10] == '.' && g->board.cells[9] == '$' && g->pushes == 1 && g->moves == 1, "undo takes the push back");
  check(undo(*g) && !undo(*g), "and back to the start, where there's nothing to undo");
  check(g->count == 0 && g->board.player == 7, "undo goes back to the start");

  // A box against a wall can't be pushed into it.
  const LevelDef wall = makeLevel("W", 3, 5, "######@$.######");
  Board w;
  build(wall, w);
  check(step(w, Dir::Right) == Step::Pushed && step(w, Dir::Right) == Step::Blocked, "a box against a wall stops");
  check(solved(w), "every box on a goal is solved");
}

// Breadth-first search over player and box positions, for the small levels the test plays.
std::string solve(const LevelDef& def) {
  Board start;
  build(def, start);
  auto key = [](const Board& b) { return std::string(b.cells, b.rows * b.cols) + char(b.player & 0xFF) + char(b.player >> 8); };
  std::map<std::string, std::pair<std::string, char>> from;
  std::queue<Board> q;
  q.push(start);
  from[key(start)] = {"", 0};
  while (!q.empty()) {
    Board b = q.front();
    q.pop();
    if (solved(b)) {
      std::string path;
      for (std::string k = key(b); !from[k].first.empty() || from[k].second;) {
        path = from[k].second + path;
        k = from[k].first;
        if (k.empty()) break;
      }
      return path;
    }
    for (Dir d : {Dir::Left, Dir::Right, Dir::Up, Dir::Down}) {
      Board n = b;
      if (step(n, d) == Step::Blocked) continue;
      const std::string k = key(n);
      if (from.count(k)) continue;
      from[k] = {key(b), "LRUD"[static_cast<int>(d)]};
      q.push(n);
    }
  }
  return "";
}

Dir dirFromLetter(char c) { return c == 'L' ? Dir::Left : c == 'R' ? Dir::Right : c == 'U' ? Dir::Up : Dir::Down; }

void testFlow() {
  auto s = fresh();
  check(s->pack == 0 && s->place[0] == 0 && s->message == Message::Title, "a fresh state opens Microban 1 with its title");
  check(!canUndo(*s) && undoMove(*s) == Change::None, "UNDO does nothing with no moves");
  check(!canReset(*s) && askReset(*s) == Change::None, "RESET does nothing on an untouched level");

  const std::string path = solve(currentLevel(*s));
  check(!path.empty(), "Microban 1 has a solution");
  Change last = Change::None;
  for (size_t i = 0; i < path.size(); i++) {
    last = go(*s, dirFromLetter(path[i]));
    if (i == 0) check(s->message == Message::None, "a move clears the title");
  }
  check(last == Change::Solved && isSolved(*s), "the solution solves it");
  check(currentBest(*s) == path.size() && s->message == Message::Solved && s->bestChanged, "the best is recorded");
  check(go(*s, Dir::Left) == Change::None, "no moves once solved");
  check(upButton(*s) == Change::None, "Up does nothing once solved");

  // Again, with a detour, beats nothing; the shortest again doesn't beat itself.
  answerReset(*s, false);
  check(askReset(*s) == Change::View && s->mode == Mode::ConfirmReset, "RESET asks first");
  check(go(*s, Dir::Left) == Change::None, "no moves while it asks");
  check(answerReset(*s, false) == Change::View && s->mode == Mode::Play && isSolved(*s), "NO keeps the level");
  askReset(*s);
  check(answerReset(*s, true) == Change::NewLevel && s->game.count == 0, "YES starts it again");
  s->bestChanged = false;
  for (char c : path) go(*s, dirFromLetter(c));
  check(s->message == Message::Solved && !s->bestChanged, "the same count isn't a new best");

  check(downButton(*s) == Change::NewLevel && s->place[0] == 1, "after a solve Down goes to the next level");
  check(downButton(*s) != Change::NewLevel, "before a solve Down moves the player");
  check(stepLevel(*s, -1) == Change::NewLevel && s->place[0] == 0, "PREV goes back");
  check(stepLevel(*s, -1) == Change::NewLevel && s->place[0] == kPacks[0].count - 1, "and wraps to the last level");
  stepLevel(*s, 1);

  check(openPacks(*s) == Change::View && s->mode == Mode::Packs, "PACKS opens the list");
  check(go(*s, Dir::Up) == Change::None, "no moves on the list");
  check(choosePack(*s, 4) == Change::NewLevel && s->pack == 4 && s->place[4] == 0, "another pack opens at its level");
  stepLevel(*s, 1);
  stepLevel(*s, 1);
  openPacks(*s);
  choosePack(*s, 0);
  check(s->pack == 0 && s->place[0] == 0, "Microban kept its place");
  openPacks(*s);
  choosePack(*s, 4);
  check(s->place[4] == 2, "and Minicosmos kept its");
  go(*s, Dir::Up);
  go(*s, Dir::Down);
  go(*s, Dir::Left);
  go(*s, Dir::Right);
  const uint16_t count = s->game.count;
  openPacks(*s);
  check(choosePack(*s, 4) == Change::View && s->mode == Mode::Play && s->game.count == count, "choosing the same pack keeps the moves");
  check(closePacks(*s) == Change::None, "BACK from the list only works on the list");

  s->game.count = kMaxHistory;
  check(go(*s, Dir::Left) == Change::View && s->message == Message::HistoryFull, "a full history says so");
  check(go(*s, Dir::Left) == Change::None, "and says it once");
}

void testDirOf() {
  check(dirOf(-30, 10) == Dir::Left && dirOf(30, -10) == Dir::Right, "across");
  check(dirOf(5, -30) == Dir::Up && dirOf(-5, 30) == Dir::Down, "up and down");
  check(dirOf(20, 20) == Dir::Right && dirOf(-20, -20) == Dir::Left, "a tie goes across, as in the source");
}

void testSave() {
  auto s = fresh();
  s->pack = 2;
  s->place[0] = 7;
  s->place[1] = 49;
  s->place[2] = 3;
  s->place[4] = 39;
  loadLevel(*s);
  const std::string path = solve(currentLevel(*s));
  for (size_t i = 0; i + 1 < path.size(); i++) go(*s, dirFromLetter(path[i]));
  const uint16_t moves = s->game.moves;
  const uint16_t pushes = s->game.pushes;

  std::vector<uint8_t> data(GAME_MAX);
  const size_t size = packGame(*s, data.data());
  check(size == GAME_HEADER + (s->game.count + 3) / 4, "four moves to a byte");

  auto back = fresh();
  check(unpackGame(data.data(), size, *back), "the save loads");
  check(back->pack == 2 && back->place[0] == 7 && back->place[1] == 49 && back->place[2] == 3 && back->place[4] == 39, "with every place");
  check(back->game.moves == moves && back->game.pushes == pushes && back->game.board.player == s->game.board.player, "and the moves replayed");
  check(std::memcmp(back->game.board.cells, s->game.board.cells, kMaxCells) == 0, "to the same board");
  check(undo(back->game), "and they can be undone");
  check(go(*back, dirFromLetter(path[path.size() - 2])) == Change::Move &&
            go(*back, dirFromLetter(path[path.size() - 1])) == Change::Solved,
        "and the level finished");

  auto refused = [&](std::vector<uint8_t> bytes, size_t n, const char* what) {
    auto t = fresh();
    t->place[3] = 11;
    check(!unpackGame(bytes.data(), n, *t) && t->place[3] == 11 && t->pack == 0, what);
  };
  refused(data, size - 1, "a short save is refused");
  refused(data, 3, "a save shorter than its header is refused");
  std::vector<uint8_t> bad = data;
  bad[0] = GAME_VERSION + 1;
  refused(bad, size, "another version is refused");
  bad = data;
  bad[1] = kPackCount;
  refused(bad, size, "a pack that doesn't exist is refused");
  bad = data;
  bad[2] = kPacks[0].count;
  refused(bad, size, "a place past the pack is refused");
  bad = data;
  bad[1] = 0;  // the moves were for LOMA 4, so they don't play on Microban 8
  refused(bad, size, "moves that can't be made are refused");

  auto done = fresh();
  for (char c : solve(currentLevel(*done))) go(*done, dirFromLetter(c));
  const size_t doneSize = packGame(*done, data.data());
  std::vector<uint8_t> extra(data.begin(), data.begin() + doneSize);
  // One more move past the solve.
  const int count = done->game.count + 1;
  extra[GAME_HEADER - 2] = count & 0xFF;
  extra[GAME_HEADER - 1] = count >> 8;
  extra.resize(GAME_HEADER + (count + 3) / 4, 0);
  refused(extra, extra.size(), "a move after the solve is refused");
  auto reloaded = fresh();
  check(unpackGame(data.data(), doneSize, *reloaded) && isSolved(*reloaded), "a solved level reloads solved");

  std::vector<uint8_t> best(BEST_BYTES);
  done->best[0] = 33;
  done->best[kTotalLevels - 1] = 1000;
  check(packBest(*done, best.data()) == BEST_BYTES, "the record is every level");
  auto record = fresh();
  check(unpackBest(best.data(), BEST_BYTES, *record) && record->best[0] == 33 && record->best[kTotalLevels - 1] == 1000, "and reloads");
  check(!unpackBest(best.data(), BEST_BYTES - 1, *record), "a short record is refused");
  best[0] = BEST_VERSION + 1;
  check(!unpackBest(best.data(), BEST_BYTES, *record), "a record of another version is refused");
}

}  // namespace

int main() {
  testEveryLevel();
  testSteps();
  testDirOf();
  testFlow();
  testSave();
  if (failures) {
    std::printf("%d failure(s)\n", failures);
    return 1;
  }
  std::printf("sokoban: all passed\n");
  return 0;
}
