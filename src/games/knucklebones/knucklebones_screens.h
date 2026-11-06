#pragma once

#include <cstdint>

#include "knucklebones_rules.h"

// Knucklebones' screens, ported from CrossPlay's KnucklebonesScreens.cpp. Each
// tappable thing has one function that says where it is, used by both the drawing
// and the tap.
namespace knuckleui {

// --- The board ---

struct BoardModel {
  const knucklebones::Grid* yours = nullptr;
  const knucklebones::Grid* theirs = nullptr;
  uint8_t die = knucklebones::kEmpty;  // the die waiting to be placed, or kEmpty
  bool yourTurn = true;
};

void drawBoard(uint8_t* frame, const BoardModel& model);

// The column of your own grid under (x, y), or -1. A whole column, as the rules
// take one, and only the cells: the gaps between columns are no column.
int columnAt(int x, int y);

// --- The menu ---

struct MenuModel {
  bool inProgress = false;  // RESUME and NEW GAME, rather than PLAY
  bool hasHistory = false;
  const knucklebones::Grid* lastYours = nullptr;
  const knucklebones::Grid* lastTheirs = nullptr;
  int wins = 0;
  int losses = 0;
  int draws = 0;
};

enum class MenuTap : uint8_t { Nothing, Play, Resume, NewGame, HowTo };

void drawMenu(uint8_t* frame, const MenuModel& model);
MenuTap menuTapAt(const MenuModel& model, int x, int y);

// --- How to play ---

int howToPages();
void drawHowTo(uint8_t* frame, int page);
bool isOnHowToNext(int x, int y);

// --- The result ---

struct ResultModel {
  int yourScore = 0;
  int theirScore = 0;
};

enum class ResultTap : uint8_t { Nothing, Again, Done };

void drawResult(uint8_t* frame, const ResultModel& model);
ResultTap resultTapAt(int x, int y);

}  // namespace knuckleui
