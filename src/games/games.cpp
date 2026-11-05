#include "games.h"

#include <cstring>

#include "tictactoe.h"

namespace games {

namespace {

Game& table(int index) {
  static Game* const GAMES[] = {
      &ticTacToe(),
  };
  return *GAMES[index];
}

constexpr int GAME_COUNT = 1;

}  // namespace

int count() { return GAME_COUNT; }

Game& at(int index) { return table(index); }

Game* find(const char* name) {
  for (int i = 0; i < GAME_COUNT; i++) {
    if (strcmp(table(i).name(), name) == 0) return &table(i);
  }
  return nullptr;
}

}  // namespace games
