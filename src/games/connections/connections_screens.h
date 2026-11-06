#pragma once

#include <cstdint>

#include "connections_rules.h"

// Connections' screens, drawn from plain values, ported from CrossPlay's
// ConnectionsScreens.cpp. Each tappable thing has one function that says where it is,
// used by both the drawing and the tap, so nothing can be drawn in one place and hit in
// another.
namespace connectionsui {

// "20260802" -> "2 AUG 2026". out holds at least 16 bytes.
void formatDate(uint32_t date, char* out, int size);

// --- The board ---

struct BoardModel {
  const connections::Game* game = nullptr;
  uint32_t date = 0;
  const char* toast = nullptr;  // "ONE AWAY" and the like, in place of MISTAKES, or null
};

void drawBoard(uint8_t* frame, const BoardModel& model);

enum class BoardTap { Nothing, Tile, Shuffle, Clear, Submit, Done };

// Sets tile for BoardTap::Tile. Done is the one wide button once the game is over.
BoardTap boardTapAt(const connections::Game& game, int x, int y, int& tile);

// --- The menu ---

// What one cell of the sixteen-day grid says.
enum class DayResult : uint8_t { Unplayed, Won, Perfect, Lost };

struct MenuModel {
  bool hasPuzzles = false;
  uint32_t newestDate = 0;
  int puzzleCount = 0;
  int played = 0;
  int perfect = 0;
  int streak = 0;
  bool inProgress = false;
  bool todayDone = false;
  // The last sixteen days, oldest first, so the last cell is the newest puzzle held.
  DayResult recent[16] = {};
  bool upToDate = false;  // the newest puzzle is today's, so there's nothing to fetch
};

enum class MenuTap { Nothing, Today, Archive, HowTo, GetPuzzles };

void drawMenu(uint8_t* frame, const MenuModel& model);
MenuTap menuTapAt(int x, int y);

void drawHowTo(uint8_t* frame);

// --- The archive calendar ---

constexpr int CALENDAR_CELLS = 42;

struct CalendarDay {
  uint8_t day = 0;  // 1-31, or 0 for a blank cell before the 1st or after the last day
  bool inArchive = false;
  bool played = false;
  bool lost = false;
  uint8_t mistakes = 0;
};

struct CalendarModel {
  int year = 2026;
  int month = 1;
  int playedThisMonth = 0;
  const CalendarDay* cells = nullptr;  // six weeks of seven, Sunday first
  int todayCell = -1;
  bool canPrevYear = false;
  bool canNextYear = false;
  bool canPrevMonth = false;
  bool canNextMonth = false;
};

enum class CalendarTap { Nothing, Today, PrevYear, NextYear, PrevMonth, NextMonth, Day };

void drawCalendar(uint8_t* frame, const CalendarModel& model);

// Sets cell for CalendarTap::Day. The gaps between cells belong to nobody, so a tap on
// a seam can't open the wrong day.
CalendarTap calendarTapAt(int x, int y, int& cell);

// --- The download screen ---

struct ImportModel {
  const char* detail = "";  // what's happening, or why it failed
  int puzzles = 0;
  bool done = false;
  bool failed = false;
};

void drawImport(uint8_t* frame, const ImportModel& model);
bool isOnImportButton(int x, int y);

}  // namespace connectionsui
