#pragma once

#include "game.h"

// The games on the home page, in the order they're listed.
namespace games {

int count();
Game& at(int index);

// Returns nullptr if no game has this name.
Game* find(const char* name);

}  // namespace games
