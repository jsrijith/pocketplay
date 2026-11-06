#pragma once

#include <cstdint>

#include "solitaire_record.h"
#include "solitaire_rules.h"

// Solitaire's screens, ported from CrossPlay's SolitaireScreens.cpp. Everything is
// drawn in landscape, 800 x 480, so all of this is plain landscape arithmetic.
namespace solitaireui {

// How much of a covered open card shows: enough for the index along its top.
constexpr int OPEN_CARD_STRIP = 30;

// --- The board ---

struct BoardModel {
  const solitaire::Game* game = nullptr;
  int selectedPile = -1;  // the run picked up, or -1 for none
  int selectedCard = 0;
};

enum class BoardHit : uint8_t { Nothing, Pile, Undo, New, Finish };

struct HitArea {
  int left = 0;
  int top = 0;
  int width = 0;
  int height = 0;
  BoardHit hit = BoardHit::Nothing;
  int pile = 0;
};

// Where everything on the board is. layoutBoard() works it out from the model, and
// both drawBoard() and the tap code read it, so a tap lands on what was drawn.
struct Layout {
  int x[solitaire::kPileCount] = {};
  int y[solitaire::kPileCount] = {};
  // How far down its column card i of a tableau pile sits.
  int cardOffset[solitaire::kTableauPiles][solitaire::kPileCapacity] = {};
  // How many waste cards are fanned out, newest on the right.
  int wasteShown = 0;
  bool hasSelection = false;
  int selectionLeft = 0;
  int selectionTop = 0;
  int selectionWidth = 0;
  int selectionHeight = 0;

  static constexpr int MAX_AREAS = 16;
  HitArea areas[MAX_AREAS];
  int areaCount = 0;

  // The topmost area under a point, or null.
  const HitArea* hitAt(int x, int y) const;

  // Which card of pile a tap at y lands on, or -1. In a fanned column it's the card
  // whose strip was touched, and below the last card still means the last card.
  int cardAt(const solitaire::Game& game, int pile, int y) const;
};

void layoutBoard(const BoardModel& model, Layout& layout);
void drawBoard(uint8_t* frame, const BoardModel& model);

// --- The menu ---

struct MenuModel {
  bool hasSave = false;
  int savedMoves = 0;
  bool drawThree = false;
  solitaire::Record record;
};

enum class MenuTap : uint8_t { Nothing, Resume, Deal, DrawMode };

void drawMenu(uint8_t* frame, const MenuModel& model);
MenuTap menuTapAt(const MenuModel& model, int x, int y);

// --- The win ---

struct WinModel {
  int moves = 0;
  int wins = 0;
  int streak = 0;
};

enum class WinTap : uint8_t { Nothing, Again, Menu };

void drawWin(uint8_t* frame, const WinModel& model);
WinTap winTapAt(int x, int y);

}  // namespace solitaireui
