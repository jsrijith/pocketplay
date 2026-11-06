#pragma once

#include <cstdint>

#include "connectfour_rules.h"

// Connect Four's screens, ported from CrossPlay's ConnectFourScreens.cpp. Each
// tappable thing has one function that says where it is, used by both the drawing
// and the tap.
namespace c4ui {

// --- The board ---

struct BoardModel {
  const connectfour::Game* game = nullptr;
  bool yourTurn = true;
};

void drawBoard(uint8_t* frame, const BoardModel& model);

// The column under (x, y), or kNoColumn: the slot at the top and the whole grid
// below it, one 64 px strip per column, frame included at the bottom.
int columnAt(int x, int y);

// --- The menu ---

struct MenuModel {
  bool inProgress = false;  // RESUME and NEW GAME, rather than PLAY
  bool hasHistory = false;
  int wins = 0;
  int losses = 0;
  int draws = 0;
  uint8_t lastOutcome = 2;           // 0 you won, 1 they won, 2 a draw
  const uint8_t* lastCells = nullptr;  // c * 6 + r, as in the record
  const int* lastLine = nullptr;     // four cells, or -1s for a draw
};

enum class MenuTap : uint8_t { Nothing, Play, Resume, NewGame, HowTo };

void drawMenu(uint8_t* frame, const MenuModel& model);
MenuTap menuTapAt(const MenuModel& model, int x, int y);

// --- How to play ---

int howToPages();
void drawHowTo(uint8_t* frame, int page);
bool isOnHowToNext(int x, int y);

// --- The result: the final board, with the winning four marked ---

struct ResultModel {
  const connectfour::Game* game = nullptr;
  const int* line = nullptr;  // the winning four, or nullptr for a draw
};

enum class ResultTap : uint8_t { Nothing, Again, Done };

void drawResult(uint8_t* frame, const ResultModel& model);
ResultTap resultTapAt(int x, int y);

}  // namespace c4ui
