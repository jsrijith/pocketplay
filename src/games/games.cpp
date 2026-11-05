#include "games.h"

#include <cstring>

#define GAME(function) Game& function();
#include "game_list.inc"
#undef GAME

namespace games {

namespace {

Game& table(int index) {
  static Game* const GAMES[] = {
#define GAME(function) &function(),
#include "game_list.inc"
#undef GAME
  };
  return *GAMES[index];
}

constexpr int GAME_COUNT = 0
#define GAME(function) +1
#include "game_list.inc"
#undef GAME
    ;

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
