#include "solitaire_screens.h"

#include <cstdio>

#include "card_art.h"
#include "core/graphics.h"
#include "core/text.h"
#include "ui/ui.h"

namespace solitaireui {

using namespace solitaire;

namespace {

// The landscape board. Seven columns across 800 px set the card size, and what's
// left of the height is the room the columns fan into.
constexpr int WIDTH = LANDSCAPE_WIDTH;
constexpr int HEIGHT = LANDSCAPE_HEIGHT;
constexpr int SIDE_MARGIN = 16;
constexpr int GUTTER = 12;
constexpr int PITCH = 112;
constexpr int CARD_W = cardart::CARD_WIDTH;
constexpr int CARD_H = cardart::CARD_HEIGHT;
constexpr int RADIUS = cardart::RADIUS;
constexpr int EDGE = cardart::EDGE;
constexpr int TOP_ROW_Y = ui::LANDSCAPE_CONTENT_TOP + GUTTER;
constexpr int TABLEAU_Y = TOP_ROW_Y + CARD_H + 24;
constexpr int BOTTOM_MARGIN = 16;

constexpr int STOCK_COLUMN = 0;
constexpr int WASTE_COLUMN = 1;
constexpr int FINISH_COLUMN = 2;
constexpr int FIRST_FOUNDATION_COLUMN = 3;
constexpr int WASTE_FAN = 36;

// UNDO and NEW sit in the band, right of the shell's warning slot, the same height
// as BACK.
constexpr int CHIP_WIDTH = 100;
constexpr int CHIP_HEIGHT = 46;
constexpr int CHIP_TOP = (ui::LANDSCAPE_BAND_HEIGHT - CHIP_HEIGHT) / 2;
constexpr int UNDO_LEFT = WIDTH - 228;
constexpr int NEW_LEFT = WIDTH - 116;
static_assert(UNDO_LEFT >= ui::LANDSCAPE_WARNING_LEFT + ui::LANDSCAPE_WARNING_WIDTH, "UNDO must clear the warning slot");

constexpr int FINISH_LEFT = SIDE_MARGIN + FINISH_COLUMN * PITCH + 22;
constexpr int FINISH_TOP = TOP_ROW_Y + (CARD_H - 56) / 2;
constexpr int FINISH_WIDTH = 82;
constexpr int FINISH_HEIGHT = 56;

constexpr int PILL_ROW_Y = HEIGHT - BOTTOM_MARGIN - ui::PILL_HEIGHT;

int columnX(int column) { return SIDE_MARGIN + column * PITCH; }

void addArea(Layout& layout, int left, int top, int width, int height, BoardHit hit, int pile = 0) {
  if (layout.areaCount >= Layout::MAX_AREAS) return;
  HitArea& area = layout.areas[layout.areaCount++];
  area.left = left;
  area.top = top;
  area.width = width;
  area.height = height;
  area.hit = hit;
  area.pile = pile;
}

// How far apart the cards in a column sit: one step for face-down cards, one for
// the top open card, and one for the open cards under it.
struct Fan {
  int down = 14;
  int firstUp = OPEN_CARD_STRIP;
  int up = OPEN_CARD_STRIP;
};

// The fan always fits, and the top card is always whole. The face-down cards, which
// only say how many there are, give way first, then the rest of the open run.
Fan fanFor(const Pile& pile, int available) {
  Fan fan;
  if (pile.count <= 1) return fan;

  int down = 0;
  for (int c = 0; c + 1 < pile.count; ++c) {
    if (!isFaceUp(pile.cards[c])) down++;
  }
  const int open = (pile.count - 1) - down;  // open cards that are covered
  const int first = open > 0 ? 1 : 0;
  const int rest = open - first;
  const auto height = [&] { return down * fan.down + first * fan.firstUp + rest * fan.up; };

  while (height() > available && fan.down > 6) fan.down--;
  while (height() > available && fan.up > 16) fan.up--;
  if (height() <= available) return fan;

  // Still too tall: everything but the top open card shares what's left.
  const int sharers = down + rest;
  if (sharers > 0) {
    const int each = (available - first * fan.firstUp) / sharers;
    fan.down = each < 3 ? 3 : each;
    fan.up = fan.down;
  }
  if (height() <= available) return fan;

  // No room even for that: an even share for every card.
  const int each = available / (pile.count - 1);
  fan.down = each < 3 ? 3 : each;
  fan.up = fan.down;
  fan.firstUp = fan.down;
  return fan;
}

// A white chip knocked out of the band, black label. One that can't act keeps its
// place and label and turns grey, as ui::drawDimmedPill does on white.
void drawChip(uint8_t* frame, int left, const char* label, bool enabled) {
  fillRoundRect(frame, left, CHIP_TOP, CHIP_WIDTH, CHIP_HEIGHT, ui::PILL_RADIUS, false);
  if (!enabled) {
    ui::drawDimmedPill(frame, left, CHIP_TOP, CHIP_WIDTH, "", CHIP_HEIGHT);
  }
  drawTextInBox(frame, Font::Size20, left, CHIP_TOP, CHIP_WIDTH, CHIP_HEIGHT, label);
}

void drawPip(uint8_t* frame, int left, int top, int width, int height, uint8_t card) {
  cardart::drawSuit(frame, left, top, width, height, suitOf(card), isRed(card));
}

// The menu's bottom row: two or three equal pills.
struct MenuRow {
  int slots = 2;
  int width = 0;
  int leftOf(int slot) const { return SIDE_MARGIN + slot * (width + GUTTER); }
};

MenuRow menuRow(const MenuModel& model) {
  MenuRow row;
  row.slots = model.hasSave ? 3 : 2;
  row.width = (WIDTH - 2 * SIDE_MARGIN - GUTTER * (row.slots - 1)) / row.slots;
  return row;
}

constexpr int WIN_BUTTON_WIDTH = 210;
constexpr int WIN_TOP = ui::LANDSCAPE_CONTENT_TOP + 27;
constexpr int WIN_BUTTON_Y = WIN_TOP + 144;
constexpr int WIN_AGAIN_LEFT = WIDTH / 2 - WIN_BUTTON_WIDTH - GUTTER / 2;
constexpr int WIN_MENU_LEFT = WIDTH / 2 + GUTTER / 2;

}  // namespace

const HitArea* Layout::hitAt(int x, int y) const {
  for (int i = areaCount - 1; i >= 0; --i) {
    const HitArea& area = areas[i];
    if (ui::isInBox(x, y, area.left, area.top, area.width, area.height)) return &area;
  }
  return nullptr;
}

int Layout::cardAt(const Game& game, int pile, int ty) const {
  if (pile < 0 || pile >= kPileCount) return -1;
  const Pile& contents = game.pile(pile);
  if (contents.empty()) return -1;
  // Only a column fans. Everywhere else only the top card can be reached.
  if (pile < kFirstTableau) return contents.count - 1;

  const int column = pile - kFirstTableau;
  const int local = ty - y[pile];
  if (local < 0) return -1;
  for (int c = contents.count - 1; c >= 0; --c) {
    if (local >= cardOffset[column][c]) return c;
  }
  return 0;
}

void layoutBoard(const BoardModel& model, Layout& layout) {
  layout = Layout();
  if (model.game == nullptr) return;
  const Game& game = *model.game;

  addArea(layout, UNDO_LEFT, CHIP_TOP, CHIP_WIDTH, CHIP_HEIGHT, BoardHit::Undo);
  addArea(layout, NEW_LEFT, CHIP_TOP, CHIP_WIDTH, CHIP_HEIGHT, BoardHit::New);

  layout.x[kStockPile] = columnX(STOCK_COLUMN);
  layout.y[kStockPile] = TOP_ROW_Y;
  addArea(layout, layout.x[kStockPile], TOP_ROW_Y, CARD_W, CARD_H, BoardHit::Pile, kStockPile);

  // Draw three shows the last three, fanned right with the newest on top.
  const Pile& waste = game.pile(kWastePile);
  layout.x[kWastePile] = columnX(WASTE_COLUMN);
  layout.y[kWastePile] = TOP_ROW_Y;
  layout.wasteShown = waste.count >= 3 ? 3 : waste.count;
  const int wasteSpan = CARD_W + (layout.wasteShown > 0 ? layout.wasteShown - 1 : 0) * WASTE_FAN;
  addArea(layout, layout.x[kWastePile], TOP_ROW_Y, wasteSpan, CARD_H, BoardHit::Pile, kWastePile);
  if (model.selectedPile == kWastePile && layout.wasteShown > 0) {
    layout.hasSelection = true;
    layout.selectionLeft = layout.x[kWastePile] + (layout.wasteShown - 1) * WASTE_FAN;
    layout.selectionTop = TOP_ROW_Y;
    layout.selectionWidth = CARD_W;
    layout.selectionHeight = CARD_H;
  }

  for (int f = 0; f < kFoundationPiles; ++f) {
    const int pile = kFirstFoundation + f;
    layout.x[pile] = columnX(FIRST_FOUNDATION_COLUMN + f);
    layout.y[pile] = TOP_ROW_Y;
    addArea(layout, layout.x[pile], TOP_ROW_Y, CARD_W, CARD_H, BoardHit::Pile, pile);
    if (model.selectedPile == pile && !game.pile(pile).empty()) {
      layout.hasSelection = true;
      layout.selectionLeft = layout.x[pile];
      layout.selectionTop = TOP_ROW_Y;
      layout.selectionWidth = CARD_W;
      layout.selectionHeight = CARD_H;
    }
  }

  // FINISH is offered only once the game is decided. See Game::canAutoFinish.
  if (game.canAutoFinish()) addArea(layout, FINISH_LEFT, FINISH_TOP, FINISH_WIDTH, FINISH_HEIGHT, BoardHit::Finish);

  const int fanRoom = HEIGHT - TABLEAU_Y - BOTTOM_MARGIN - CARD_H;
  for (int column = 0; column < kTableauPiles; ++column) {
    const int pile = kFirstTableau + column;
    const Pile& contents = game.pile(pile);
    const int x = columnX(column);
    layout.x[pile] = x;
    layout.y[pile] = TABLEAU_Y;
    if (contents.empty()) {
      addArea(layout, x, TABLEAU_Y, CARD_W, CARD_H, BoardHit::Pile, pile);
      continue;
    }

    const Fan fan = fanFor(contents, fanRoom);
    int offset = 0;
    bool seenOpen = false;
    for (int c = 0; c < contents.count; ++c) {
      layout.cardOffset[column][c] = offset;
      int step = fan.down;
      if (isFaceUp(contents.cards[c])) {
        step = seenOpen ? fan.up : fan.firstUp;
        seenOpen = true;
      }
      if (c + 1 < contents.count) offset += step;
    }

    const int bottom = TABLEAU_Y + layout.cardOffset[column][contents.count - 1] + CARD_H;
    if (model.selectedPile == pile && model.selectedCard >= 0 && model.selectedCard < contents.count) {
      layout.hasSelection = true;
      layout.selectionLeft = x;
      layout.selectionTop = TABLEAU_Y + layout.cardOffset[column][model.selectedCard];
      layout.selectionWidth = CARD_W;
      layout.selectionHeight = bottom - layout.selectionTop;
    }
    // One area per column. Which card was touched comes from cardAt().
    addArea(layout, x, TABLEAU_Y, CARD_W, bottom - TABLEAU_Y, BoardHit::Pile, pile);
  }
}

void drawBoard(uint8_t* frame, const BoardModel& model) {
  if (model.game == nullptr) return;
  const Game& game = *model.game;
  Layout layout;
  layoutBoard(model, layout);

  // The two standing actions live in the band, so they cost the cards no room.
  ui::drawBand(frame, "SOLITAIRE", nullptr, true);
  drawChip(frame, UNDO_LEFT, "UNDO", game.canUndo());
  drawChip(frame, NEW_LEFT, "NEW", true);

  // An empty stock wears a ring: the next tap goes round the waste again.
  const int stockX = layout.x[kStockPile];
  if (game.pile(kStockPile).empty()) {
    cardart::drawCardSlot(frame, stockX, TOP_ROW_Y, CARD_W, CARD_H);
    constexpr int RING = 44;
    drawRing(frame, stockX + CARD_W / 2, TOP_ROW_Y + CARD_H / 2, RING / 2, ui::RULE, true);
  } else {
    cardart::drawCardBack(frame, stockX, TOP_ROW_Y, CARD_W, CARD_H, CARD_H);
  }

  const Pile& waste = game.pile(kWastePile);
  const int wasteX = layout.x[kWastePile];
  if (waste.empty()) {
    cardart::drawCardSlot(frame, wasteX, TOP_ROW_Y, CARD_W, CARD_H);
  } else {
    // A covered waste card is covered from the right, so its index runs down the
    // sliver that shows.
    const int shown = layout.wasteShown;
    for (int i = shown - 1; i >= 0; --i) {
      const int left = wasteX + (shown - 1 - i) * WASTE_FAN;
      const bool top = i == 0;
      cardart::drawCardFace(frame, left, TOP_ROW_Y, CARD_W, CARD_H, waste.cards[waste.count - 1 - i], top ? CARD_H : WASTE_FAN,
                            top ? cardart::Fan::Down : cardart::Fan::Sideways);
    }
  }

  for (int f = 0; f < kFoundationPiles; ++f) {
    const int pile = kFirstFoundation + f;
    const Pile& contents = game.pile(pile);
    if (contents.empty()) {
      cardart::drawCardSlot(frame, layout.x[pile], TOP_ROW_Y, CARD_W, CARD_H);
    } else {
      cardart::drawCardFace(frame, layout.x[pile], TOP_ROW_Y, CARD_W, CARD_H, contents.top(), CARD_H, cardart::Fan::Down);
    }
  }

  if (game.canAutoFinish()) {
    fillRoundRect(frame, FINISH_LEFT, FINISH_TOP, FINISH_WIDTH, FINISH_HEIGHT, ui::PILL_RADIUS, true);
    drawTextInBox(frame, Font::Size14, FINISH_LEFT, FINISH_TOP, FINISH_WIDTH, FINISH_HEIGHT, "FINISH", false);
  }

  for (int column = 0; column < kTableauPiles; ++column) {
    const int pile = kFirstTableau + column;
    const Pile& contents = game.pile(pile);
    const int x = layout.x[pile];
    if (contents.empty()) {
      // An empty column takes only a king.
      cardart::drawCardSlot(frame, x, TABLEAU_Y, CARD_W, CARD_H);
      continue;
    }
    for (int c = 0; c < contents.count; ++c) {
      const int top = TABLEAU_Y + layout.cardOffset[column][c];
      const int visible = c + 1 < contents.count ? layout.cardOffset[column][c + 1] - layout.cardOffset[column][c] : CARD_H;
      if (isFaceUp(contents.cards[c])) {
        cardart::drawCardFace(frame, x, top, CARD_W, CARD_H, contents.cards[c], visible, cardart::Fan::Down);
      } else {
        cardart::drawCardBack(frame, x, top, CARD_W, CARD_H, visible);
      }
    }
  }

  // The selection last, so it sits over every card it encloses.
  if (layout.hasSelection) {
    drawRoundRectOutline(frame, layout.selectionLeft, layout.selectionTop, layout.selectionWidth, layout.selectionHeight, RADIUS,
                         ui::FRAME + EDGE);
  }
}

// --- The menu ---

void drawMenu(uint8_t* frame, const MenuModel& model) {
  ui::drawBand(frame, "SOLITAIRE", nullptr, true);

  // Two columns, because 480 px of height won't take the portrait front door. The
  // left says where play stands, and the right is the record.
  constexpr int LEFT = SIDE_MARGIN;
  constexpr int COLUMN_WIDTH = 420;
  constexpr int TOP = ui::LANDSCAPE_CONTENT_TOP + 33;
  const Record& record = model.record;

  ui::drawTextLeft(frame, Font::Size30, LEFT, TOP, 56, model.hasSave ? "DEAL IN PLAY" : "FRESH DECK");

  char line[64];
  if (model.hasSave) {
    snprintf(line, sizeof(line), "%d MOVES DEEP", model.savedMoves);
  } else {
    snprintf(line, sizeof(line), "SHUFFLED AND WAITING");
  }
  ui::drawTextLeft(frame, Font::Size20, LEFT, TOP + 60, 26, line);

  if (record.played == 0) {
    snprintf(line, sizeof(line), "NEVER PLAYED. NO SHAME IN THAT.");
  } else {
    snprintf(line, sizeof(line), "%d DEALT, %d CLEARED, %d IN A ROW", record.played, record.wins, record.streak);
  }
  ui::drawTextLeft(frame, Font::Size10, LEFT, TOP + 100, 24, line);

  // The record as a hand of sixteen: face up for a deal cleared, face down for one
  // walked away from, an outline for one not played yet.
  constexpr int HAND_X = LEFT + COLUMN_WIDTH + 20;
  constexpr int HAND_Y = TOP + 10;
  constexpr int MINI_W = 52;
  constexpr int MINI_H = 74;
  constexpr int MINI_STEP = 26;
  for (int i = 0; i < RECENT_GAMES; ++i) {
    const int x = HAND_X + (i % 8) * MINI_STEP;
    const int y = HAND_Y + (i / 8) * (MINI_H + 18);
    fillRoundRect(frame, x, y, MINI_W, MINI_H, 5, false);
    if (record.recent[i] == RESULT_WON) {
      drawRoundRectOutline(frame, x, y, MINI_W, MINI_H, 5, EDGE);
      drawPip(frame, x + 7, y + 10, 18, 20, makeCard(static_cast<Suit>(i % kSuits), 0));
    } else if (record.recent[i] == RESULT_LOST) {
      cardart::fillDarkGrey(frame, x + 4, y + 4, MINI_W - 8, MINI_H - 8, 3);
      drawRoundRectOutline(frame, x, y, MINI_W, MINI_H, 5, EDGE);
    } else {
      drawRoundRectOutline(frame, x, y, MINI_W, MINI_H, 5, ui::HAIRLINE);
    }
  }
  ui::drawTextLeft(frame, Font::Size10, HAND_X, HAND_Y + (MINI_H + 22) * 2 + 2, 24,
                   record.played == 0 ? "SIXTEEN DEALS TO FILL" : "YOUR LAST SIXTEEN");

  const MenuRow row = menuRow(model);
  int slot = 0;
  if (model.hasSave) ui::drawPill(frame, row.leftOf(slot++), PILL_ROW_Y, row.width, "PICK IT UP", true);
  ui::drawPill(frame, row.leftOf(slot++), PILL_ROW_Y, row.width, model.hasSave ? "DEAL FRESH" : "DEAL", !model.hasSave);
  // A setting, not an action, so it says which way it points. It takes effect on
  // the next deal, which is why it can sit beside PICK IT UP.
  ui::drawPill(frame, row.leftOf(slot), PILL_ROW_Y, row.width, model.drawThree ? "TURNING THREE" : "TURNING ONE", false);
}

MenuTap menuTapAt(const MenuModel& model, int x, int y) {
  const MenuRow row = menuRow(model);
  int slot = 0;
  if (model.hasSave && ui::isInBox(x, y, row.leftOf(slot++), PILL_ROW_Y, row.width, ui::PILL_HEIGHT)) return MenuTap::Resume;
  if (ui::isInBox(x, y, row.leftOf(slot++), PILL_ROW_Y, row.width, ui::PILL_HEIGHT)) return MenuTap::Deal;
  if (ui::isInBox(x, y, row.leftOf(slot), PILL_ROW_Y, row.width, ui::PILL_HEIGHT)) return MenuTap::DrawMode;
  return MenuTap::Nothing;
}

// --- The win ---

void drawWin(uint8_t* frame, const WinModel& model) {
  ui::drawBand(frame, "SOLITAIRE", nullptr, true);

  // Where the deck lands when it comes off the foundations: a bounce of cards
  // across the foot of the screen, lowest in the middle.
  constexpr int CASCADE = 28;
  constexpr int MINI_W = 58;
  constexpr int MINI_H = 86;
  constexpr int STEP = (WIDTH - MINI_W - 36) / (CASCADE - 1);
  for (int i = 0; i < CASCADE; ++i) {
    const int fromCentre = i - CASCADE / 2;
    const int lift = (fromCentre * fromCentre * 30) / ((CASCADE / 2) * (CASCADE / 2));
    const int x = 18 + i * STEP;
    const int y = HEIGHT - BOTTOM_MARGIN - MINI_H - lift;
    fillRoundRect(frame, x, y, MINI_W, MINI_H, 6, false);
    drawRoundRectOutline(frame, x, y, MINI_W, MINI_H, 6, ui::HAIRLINE);
    // In the strip the next card doesn't cover.
    if (i % 3 == 0) drawPip(frame, x + 3, y + 24, 20, 24, makeCard(static_cast<Suit>((i / 3) % kSuits), 0));
  }

  drawTextInBox(frame, Font::Size30, 0, WIN_TOP, WIDTH, 64, "ALL FIFTY-TWO HOME");

  // Varied by the move count, so the second win doesn't read like the first.
  static const char* const BRAGS[4] = {
      "%d MOVES. THE DECK NEVER STOOD A CHANCE.",
      "%d MOVES, AND NOT ONE OF THEM WASTED.",
      "%d MOVES. SOMEBODY HAS DONE THIS BEFORE.",
      "%d MOVES. THE STOCK IS STILL RECOVERING.",
  };
  char line[96];
  snprintf(line, sizeof(line), BRAGS[model.moves % 4], model.moves);
  drawTextInBox(frame, Font::Size20, 0, WIN_TOP + 70, WIDTH, 26, line);

  snprintf(line, sizeof(line), "%d CLEARED, %d IN A ROW", model.wins, model.streak);
  drawTextInBox(frame, Font::Size10, 0, WIN_TOP + 104, WIDTH, 24, line);

  ui::drawPill(frame, WIN_AGAIN_LEFT, WIN_BUTTON_Y, WIN_BUTTON_WIDTH, "DEAL AGAIN", true);
  ui::drawPill(frame, WIN_MENU_LEFT, WIN_BUTTON_Y, WIN_BUTTON_WIDTH, "BACK TO THE TABLE", false);
}

WinTap winTapAt(int x, int y) {
  if (ui::isInBox(x, y, WIN_AGAIN_LEFT, WIN_BUTTON_Y, WIN_BUTTON_WIDTH, ui::PILL_HEIGHT)) return WinTap::Again;
  if (ui::isInBox(x, y, WIN_MENU_LEFT, WIN_BUTTON_Y, WIN_BUTTON_WIDTH, ui::PILL_HEIGHT)) return WinTap::Menu;
  return WinTap::Nothing;
}

}  // namespace solitaireui
