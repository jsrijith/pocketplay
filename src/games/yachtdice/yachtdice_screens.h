#pragma once

#include <cstdint>

#include "yachtdice_rules.h"

// Yacht Dice's screens, ported from CrossPlay's YahtzeeScreens.cpp. Each tappable
// thing has one function that says where it is, used by both the drawing and the tap.
namespace yachtui {

// --- The card ---

struct CardModel {
  const yahtzee::Game* game = nullptr;
  uint8_t seat = 0;           // which card is yours
  bool yourTurn = true;
  uint16_t takeable = 0;      // the boxes you may fill now, a bit each
  bool joker = false;         // a five of a kind that forces one box
};

void drawCard(uint8_t* frame, const CardModel& model);

// The die under (x, y), or -1. The gap between two dice is neither.
int dieAt(int x, int y);
// The box's row under (x, y), as a yahtzee::Category, or -1 for the bonus line, the
// total or outside the card.
int categoryAt(int x, int y);
bool isOnAction(int x, int y);

// --- The menu ---

struct MenuModel {
  bool inProgress = false;  // RESUME and NEW GAME, rather than PLAY
  int played = 0;
  int won = 0;
  int best = 0;
  int fives = 0;     // five of a kinds rolled
  int lastFace = 0;  // the last one's face, or 0
  // A line above the rows, such as why PLAY NEARBY didn't start, or nullptr.
  const char* notice = nullptr;
};

enum class MenuTap : uint8_t { Nothing, Play, Resume, NewGame, Nearby, HowTo };

void drawMenu(uint8_t* frame, const MenuModel& model);
MenuTap menuTapAt(const MenuModel& model, int x, int y);

// --- How to play ---

int howToPages();
void drawHowTo(uint8_t* frame, int page);
bool isOnHowToNext(int x, int y);

// --- The result ---

struct ResultModel {
  const yahtzee::Card* yours = nullptr;
  const yahtzee::Card* theirs = nullptr;
};

enum class ResultTap : uint8_t { Nothing, Again, Done };

void drawResult(uint8_t* frame, const ResultModel& model);
ResultTap resultTapAt(int x, int y);

}  // namespace yachtui
