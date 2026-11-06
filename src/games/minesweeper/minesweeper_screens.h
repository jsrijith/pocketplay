#pragma once

#include <cstdint>

#include "minesweeper_rules.h"

// Minesweeper's screens, ported from CrossPlay's MinesweeperScreens.cpp. Each
// tappable thing has one function that says where it is, used by both the drawing
// and the tap.
namespace mineui {

// --- The board ---

// One cell's top-left corner, and its exact inverse. cellAt() is false off the board.
void cellOrigin(int column, int row, int& left, int& top);
bool cellAt(int x, int y, int& column, int& row);
constexpr int CELL = 60;

struct BoardModel {
  const minesweeper::Game* game = nullptr;
  int holdColumn = -1;  // the cell under a finger while a hold builds, or -1
  int holdRow = -1;
  bool flagMode = false;
};

void drawBoard(uint8_t* frame, const BoardModel& model);

// The strip under the board: DIG/FLAG while playing, and the verdict once settled.
bool isOnMode(int x, int y);
bool isOnVerdict(int x, int y);

// --- The menu ---

struct MenuModel {
  bool inProgress = false;  // RESUME and NEW GAME, rather than PLAY
  bool hasHistory = false;
  const minesweeper::Game* lastBoard = nullptr;
  int wins = 0;
  int losses = 0;
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
  bool won = false;
  int revealed = 0;
  int flagsRight = 0;
};

enum class ResultTap : uint8_t { Nothing, Again, Done };

void drawResult(uint8_t* frame, const ResultModel& model);
ResultTap resultTapAt(int x, int y);

}  // namespace mineui
