#pragma once

#include <cstdint>

#include "sudoku_game.h"

// Sudoku's screens, ported from CrossPlay's SudokuScreens.cpp. The grid and the pad
// are hit-tested from the same functions that draw them.
namespace sudokuui {

constexpr int CELL = 50;

// One cell's top-left corner, and its exact inverse. cellAt() is false off the grid.
void cellOrigin(int cell, int& left, int& top);
bool cellAt(int x, int y, int& cell);

// The digit pad: three by three, because a Sudoku box is. Its keys abut, so every
// pixel of the pad belongs to exactly one digit.
bool padKeyAt(int x, int y, int& digit);

enum class RailTap : uint8_t { Nothing, Status, Undo, Hint };
RailTap railTapAt(int x, int y);

struct BoardModel {
  const sudoku::Game* game = nullptr;
  int holdCell = sudoku::kNoCell;  // the cell under a finger while a hold builds
  bool generating = false;
  const char* notice = nullptr;
};

void drawBoard(uint8_t* frame, const BoardModel& model);

struct MenuModel {
  sudoku::Level level = sudoku::Level::Easy;
  bool hasGame = false;
  const sudoku::Game* game = nullptr;
  const sudoku::Record* record = nullptr;
  bool levelSelected = false;  // DIFFICULTY was the last row tapped
};

enum class MenuTap : uint8_t { Nothing, Play, Level, HowTo };

void drawMenu(uint8_t* frame, const MenuModel& model);
MenuTap menuTapAt(int x, int y);

int howToPages();
void drawHowTo(uint8_t* frame, int page);
bool isOnHowToNext(int x, int y);

struct ResultModel {
  const sudoku::Game* game = nullptr;
  uint32_t bestMs = 0;
  bool newBest = false;
  int solvedAtThisLevel = 0;
};

enum class ResultTap : uint8_t { Nothing, Another, Grid };

void drawResult(uint8_t* frame, const ResultModel& model);
ResultTap resultTapAt(int x, int y);

// "4:07", or "1:04:07" past an hour.
void formatClock(uint32_t ms, char* out, int size);

}  // namespace sudokuui
