#pragma once

#include <cstdint>

#include "wordle_rules.h"

// Wordle's screens, drawn from plain values. Each tappable thing has one function that
// says where it is, used by both the drawing and the tap, so a key can't be drawn in
// one place and hit in another.
namespace wordleui {

// Keys that aren't letters.
constexpr char KEY_ENTER = '\n';
constexpr char KEY_ERASE = '\b';

// --- The game ---

struct GameModel {
  const wordle::Game* game = nullptr;
  const char* date = nullptr;     // "SEP 28", shown in the header while playing
  const char* message = nullptr;  // one line under the header, or null
  bool canNext = false;           // a finished game: is there a day left to play
};

void drawGame(uint8_t* frame, const GameModel& model);

// The key under a tap: 'A'-'Z', KEY_ENTER, KEY_ERASE, or 0 for a gap or outside.
char keyAt(int x, int y);
bool isOnFinishedArchive(int x, int y);
bool isOnFinishedNext(int x, int y);

// --- The menu ---

struct MenuModel {
  const char* date = nullptr;  // "28 SEP 2026", or null before the first download
  const char* state = "NOT STARTED";
  wordle::Stats stats;
  int puzzles = 0;
  bool upToDate = false;
};

enum class MenuTap { Nothing, Today, HowTo, Archive, GetPuzzles };

void drawMenu(uint8_t* frame, const MenuModel& model);
MenuTap menuTapAt(int x, int y);

void drawHowTo(uint8_t* frame);

// --- The archive calendar ---

constexpr int CALENDAR_CELLS = 42;

struct CalendarDay {
  uint8_t day = 0;  // 0 is a blank cell before the 1st or after the last day
  bool inArchive = false;
  bool played = false;
  bool finished = false;
  bool lost = false;
};

struct CalendarModel {
  int year = 2026;
  int month = 1;
  int playedThisMonth = 0;
  const CalendarDay* cells = nullptr;
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

}  // namespace wordleui
