#include "yachtdice_screens.h"

#include <cstdio>

#include "core/graphics.h"
#include "core/text.h"
#include "ui/ui.h"
#include "yachtdice_flow.h"

namespace yachtui {

namespace {

namespace yz = yahtzee;

constexpr const char* TITLE = "YACHT DICE";

constexpr int SIDE = 16;
constexpr int BODY_WIDTH = SCREEN_WIDTH - 2 * SIDE;
constexpr int BOTTOM = SCREEN_HEIGHT - 16;
constexpr int PILL_TOP = BOTTOM - ui::PILL_HEIGHT;
constexpr int HALF_WIDTH = (BODY_WIDTH - ui::GAP) / 2;

// --- Dice ---

// 56 px with 18 between, where CrossPlay has 62 and 12: a held die is framed outside
// its edge, and at 12 two held neighbours' frames fell on the same pixels.
constexpr int DIE = 56;
constexpr int DIE_GAP = 18;
constexpr int DICE_WIDTH = yz::kDice * DIE + (yz::kDice - 1) * DIE_GAP;
constexpr int DICE_LEFT = (SCREEN_WIDTH - DICE_WIDTH) / 2;
// The held frame: 3 px of air, then a 4 px line, outside the die.
constexpr int HOLD_AIR = 3;
constexpr int HOLD_OUTSET = HOLD_AIR + ui::FRAME;
// The frame reaches up to CONTENT_TOP and no further, clear of the warning line.
constexpr int DICE_TOP = ui::CONTENT_TOP + HOLD_OUTSET;
static_assert(2 * HOLD_OUTSET < DIE_GAP, "two held dice's frames must not touch");

int dieLeft(int index) { return DICE_LEFT + index * (DIE + DIE_GAP); }

// A die's pips in a 3x3 lattice, as CrossPlay prints them.
void drawDie(uint8_t* frame, int left, int top, int face, bool held) {
  fillRect(frame, left, top, DIE, DIE, false);
  drawRectOutline(frame, left, top, DIE, DIE, ui::RULE);
  static const uint16_t PIPS[7] = {0, 0b000010000, 0b100000001, 0b100010001, 0b101000101, 0b101010101, 0b101101101};
  constexpr int PIP = 8;
  constexpr int INSET = 12;
  const int xs[3] = {left + INSET, left + (DIE - PIP) / 2, left + DIE - INSET - PIP};
  const int ys[3] = {top + INSET, top + (DIE - PIP) / 2, top + DIE - INSET - PIP};
  const uint16_t bits = PIPS[face >= 1 && face <= yz::kFaces ? face : 1];
  for (int row = 0; row < 3; ++row) {
    for (int column = 0; column < 3; ++column) {
      if ((bits & (1u << (8 - (row * 3 + column)))) != 0) fillRect(frame, xs[column], ys[row], PIP, PIP, true);
    }
  }
  if (held) {
    drawRectOutline(frame, left - HOLD_OUTSET, top - HOLD_OUTSET, DIE + 2 * HOLD_OUTSET, DIE + 2 * HOLD_OUTSET,
                    ui::FRAME);
  }
}

void drawDiceRow(uint8_t* frame, int top, const uint8_t* faces, uint8_t held) {
  for (int i = 0; i < yz::kDice; ++i) drawDie(frame, dieLeft(i), top, faces[i], (held & (1 << i)) != 0);
}

// --- The card: fifteen lines, six upper, the bonus, seven lower, the total ---

constexpr int HEADER_TOP = DICE_TOP + DIE + HOLD_OUTSET + 4;
constexpr int HEADER_HEIGHT = 20;
constexpr int TABLE_TOP = HEADER_TOP + HEADER_HEIGHT;
// 34 px, where CrossPlay has 36: PocketPlay's band and warning line start the card
// lower, and the ROLL pill keeps its 12 px of air.
constexpr int LINE = 34;
constexpr int BONUS_LINE = yz::kUpperEnd;
constexpr int TOTAL_LINE = yz::kCategories + 1;
constexpr int LINES = yz::kCategories + 2;
static_assert(TABLE_TOP + LINES * LINE + ui::RULE + ui::GAP <= PILL_TOP, "the card has run into the ROLL pill");

// Where the two columns end. The names have everything to their left.
constexpr int YOUR_RIGHT = 336;
constexpr int THEIR_RIGHT = SCREEN_WIDTH - SIDE;
constexpr int BOX_WIDTH = 62;
constexpr int BOX_INSET = 4;  // above and below the box within its line
constexpr int NUMBER_PAD = 12;  // a number's right edge inside its box

constexpr int lineOf(int category) { return category < yz::kUpperEnd ? category : category + 1; }
constexpr int lineTop(int line) { return TABLE_TOP + line * LINE; }

const char* categoryName(int index) {
  // FIVE OF A KIND where CrossPlay prints the trademark.
  static const char* const NAMES[yz::kCategories] = {
      "ONES",           "TWOS",       "THREES",         "FOURS",          "FIVES",          "SIXES",  "THREE OF A KIND",
      "FOUR OF A KIND", "FULL HOUSE", "SMALL STRAIGHT", "LARGE STRAIGHT", "FIVE OF A KIND", "CHANCE",
  };
  return NAMES[index];
}

// Right-aligned, with its capitals centred in the box's height.
void drawRight(uint8_t* frame, Font font, int right, int top, int height, const char* text) {
  drawText(frame, font, right, top + (height + capHeight(font)) / 2, text, Align::Right);
}

void drawNumber(uint8_t* frame, Font font, int right, int top, int value) {
  char text[8];
  snprintf(text, sizeof(text), "%d", value);
  drawRight(frame, font, right - NUMBER_PAD, top, LINE, text);
}

// The printed card's empty box. A filled box has no outline, so a committed score
// and a pencilled one never look alike.
void drawEmptyBox(uint8_t* frame, int right, int top) {
  drawRectOutline(frame, right - BOX_WIDTH, top + BOX_INSET, BOX_WIDTH, LINE - 2 * BOX_INSET, ui::RULE);
}

// What you would score in a box: the whole decision each turn.
void drawPreview(uint8_t* frame, int right, int top, int value) {
  drawEmptyBox(frame, right, top);
  drawNumber(frame, Font::Size14, right, top, value);
}

const char* actionLabel(const CardModel& model, char* buffer, size_t size, bool& live) {
  live = false;
  if (!model.yourTurn) return "THEIR TURN";
  const yz::Stage stage = yz::stageOf(*model.game);
  if (stage == yz::Stage::Spent) return model.joker ? "FIVE OF A KIND! TAKE THE MARKED BOX" : "TAKE A BOX";
  live = true;
  if (model.joker) return "FIVE OF A KIND! ONE BOX ONLY";
  const int left = yz::kRollsPerTurn - model.game->rollsUsed;
  if (left == yz::kRollsPerTurn) return "ROLL";
  snprintf(buffer, size, "ROLL AGAIN (%d LEFT)", left);
  return buffer;
}

}  // namespace

void drawCard(uint8_t* frame, const CardModel& model) {
  ui::drawBand(frame, TITLE, nullptr, true);
  const yz::Game& game = *model.game;
  const yz::Card& yours = game.card[model.seat];
  const yz::Card& theirs = game.card[1 - model.seat];

  // Before a turn's first roll the dice are the last player's and mean nothing, so
  // they aren't drawn.
  if (yz::stageOf(game) != yz::Stage::Fresh) {
    drawDiceRow(frame, DICE_TOP, game.die, game.held);
  } else {
    drawTextInBox(frame, Font::Size20, SIDE, DICE_TOP, BODY_WIDTH, DIE,
                  model.yourTurn ? "ROLL TO START YOUR TURN" : "THEY ARE ROLLING");
  }

  drawRight(frame, Font::Size14, YOUR_RIGHT - NUMBER_PAD, HEADER_TOP, HEADER_HEIGHT, "YOU");
  drawRight(frame, Font::Size14, THEIR_RIGHT - NUMBER_PAD, HEADER_TOP, HEADER_HEIGHT, "THEM");

  for (int index = 0; index < yz::kCategories; ++index) {
    const int top = lineTop(lineOf(index));
    ui::drawTextLeft(frame, Font::Size14, SIDE, top, LINE, categoryName(index));
    if (yours.box[index] != yz::kUnscored) {
      drawNumber(frame, Font::Size14, YOUR_RIGHT, top, yours.box[index]);
    } else if ((model.takeable & (1u << index)) != 0) {
      drawPreview(frame, YOUR_RIGHT, top, yz::scoreFor(yours, game.die, static_cast<yz::Category>(index)));
    } else {
      drawEmptyBox(frame, YOUR_RIGHT, top);
    }
    if (theirs.box[index] != yz::kUnscored) {
      drawNumber(frame, Font::Size14, THEIR_RIGHT, top, theirs.box[index]);
    } else {
      drawEmptyBox(frame, THEIR_RIGHT, top);
    }
  }

  // The bonus line, doing the top half's arithmetic for both players.
  const int bonusTop = lineTop(BONUS_LINE);
  fillRect(frame, SIDE, bonusTop, BODY_WIDTH, ui::HAIRLINE, true);
  ui::drawTextLeft(frame, Font::Size14, SIDE, bonusTop, LINE, "BONUS");
  for (int column = 0; column < 2; ++column) {
    const yz::Card& card = column == 0 ? yours : theirs;
    char cell[16];
    if (yz::bonusEarned(card) > 0) {
      snprintf(cell, sizeof(cell), "+%d", yz::kUpperBonus);
    } else if (yz::upperBonusStillPossible(card)) {
      snprintf(cell, sizeof(cell), "%d MORE", yz::upperShortfall(card));
    } else {
      snprintf(cell, sizeof(cell), "GONE");
    }
    drawRight(frame, Font::Size14, (column == 0 ? YOUR_RIGHT : THEIR_RIGHT) - NUMBER_PAD, bonusTop, LINE, cell);
  }

  // The extra five of a kinds, on that box's line: 100 each, the biggest swing in
  // the game, which no box shows.
  if (yours.yahtzeeBonuses > 0 || theirs.yahtzeeBonuses > 0) {
    const int index = static_cast<int>(yz::Category::Yahtzee);
    char extra[16];
    snprintf(extra, sizeof(extra), "+%d", (yours.yahtzeeBonuses + theirs.yahtzeeBonuses) * yz::kYahtzeeBonus);
    ui::drawTextLeft(frame, Font::Size14, SIDE + textWidth(Font::Size14, categoryName(index)) + 10,
                     lineTop(lineOf(index)), LINE, extra);
  }

  const int totalTop = lineTop(TOTAL_LINE);
  fillRect(frame, SIDE, totalTop, BODY_WIDTH, ui::RULE, true);
  ui::drawTextLeft(frame, Font::Size20, SIDE, totalTop + ui::RULE, LINE, "TOTAL");
  drawNumber(frame, Font::Size20, YOUR_RIGHT, totalTop + ui::RULE, yz::total(yours));
  drawNumber(frame, Font::Size20, THEIR_RIGHT, totalTop + ui::RULE, yz::total(theirs));

  // One pill, two jobs that are never both live: ROLL while a roll is left, and
  // what to do once none is. It dims when it can't act.
  char buffer[32];
  bool live = false;
  const char* label = actionLabel(model, buffer, sizeof(buffer), live);
  if (live) {
    ui::drawPill(frame, SIDE, PILL_TOP, BODY_WIDTH, label, true);
  } else {
    ui::drawDimmedPill(frame, SIDE, PILL_TOP, BODY_WIDTH, label);
  }
}

int dieAt(int x, int y) {
  // The held frame counts as the die, so tapping a frame releases it.
  if (y < DICE_TOP - HOLD_OUTSET || y >= DICE_TOP + DIE + HOLD_OUTSET) return -1;
  for (int i = 0; i < yz::kDice; ++i) {
    if (x >= dieLeft(i) - HOLD_OUTSET && x < dieLeft(i) + DIE + HOLD_OUTSET) return i;
  }
  return -1;
}

int categoryAt(int x, int y) {
  if (x < SIDE || x >= SCREEN_WIDTH - SIDE || y < TABLE_TOP) return -1;
  const int line = (y - TABLE_TOP) / LINE;
  if (line >= LINES || line == BONUS_LINE || line == TOTAL_LINE) return -1;
  return line < BONUS_LINE ? line : line - 1;
}

bool isOnAction(int x, int y) { return ui::isInBox(x, y, SIDE, PILL_TOP, BODY_WIDTH, ui::PILL_HEIGHT); }

// --- The menu ---

namespace {

struct MenuRows {
  int count = 2;
  const char* labels[3] = {};
  MenuTap taps[3] = {};
  int topOf(int i) const { return BOTTOM - ui::ROW_HEIGHT - (count - 1 - i) * (ui::ROW_HEIGHT + ui::ROW_GAP); }
};

MenuRows menuRows(const MenuModel& model) {
  MenuRows rows;
  if (model.inProgress) {
    rows.count = 3;
    rows.labels[0] = "RESUME";
    rows.taps[0] = MenuTap::Resume;
    rows.labels[1] = "NEW GAME";
    rows.taps[1] = MenuTap::NewGame;
  } else {
    rows.labels[0] = "PLAY";
    rows.taps[0] = MenuTap::Play;
  }
  rows.labels[rows.count - 1] = "HOW TO PLAY";
  rows.taps[rows.count - 1] = MenuTap::HowTo;
  return rows;
}

// The first row reads selected, as CrossPlay's does: the likeliest tap is also the
// loudest thing below the rule.
void drawSelectedRow(uint8_t* frame, int top, const char* label) {
  fillRect(frame, ui::MARGIN, top, SCREEN_WIDTH - 2 * ui::MARGIN, ui::ROW_HEIGHT, true);
  ui::drawTextLeft(frame, Font::Size20, ui::MARGIN + 14, top, ui::ROW_HEIGHT, label, false);
}

}  // namespace

void drawMenu(uint8_t* frame, const MenuModel& model) {
  ui::drawBand(frame, TITLE, nullptr, true);

  char record[48];
  snprintf(record, sizeof(record), "%d PLAYED   %d WON", model.played, model.won);
  constexpr int RECORD_HEIGHT = 26;
  ui::drawTextLeft(frame, Font::Size10, SIDE, ui::CONTENT_TOP, RECORD_HEIGHT, record);
  const int ruleTop = ui::CONTENT_TOP + RECORD_HEIGHT + 6;
  fillRect(frame, SIDE, ruleTop, BODY_WIDTH, ui::RULE, true);

  const MenuRows rows = menuRows(model);
  for (int i = 0; i < rows.count; i++) {
    if (i == 0) {
      drawSelectedRow(frame, rows.topOf(i), rows.labels[i]);
    } else {
      ui::drawRow(frame, rows.topOf(i), rows.labels[i], nullptr);
    }
  }
  if (model.played == 0) return;

  // The personal best, and under it the hand of the last five of a kind, once there
  // is one: the player's own numbers, so a new device shows only the record.
  const bool hasHand = model.fives > 0 && model.lastFace >= 1 && model.lastFace <= yz::kFaces;
  constexpr int LABEL = 26;
  constexpr int BEST = 90;
  constexpr int CAPTION = 26;
  const int block = LABEL + BEST + (hasHand ? 18 + DIE + 12 + CAPTION : 0);
  const int roomTop = ruleTop + ui::RULE;
  const int room = rows.topOf(0) - 12 - roomTop;
  const int top = roomTop + (room > block ? (room - block) / 2 : 12);

  drawTextInBox(frame, Font::Size14, SIDE, top, BODY_WIDTH, LABEL, "PERSONAL BEST");
  char best[8];
  snprintf(best, sizeof(best), "%d", model.best);
  drawTextInBox(frame, Font::Size64, SIDE, top + LABEL, BODY_WIDTH, BEST, best);
  if (!hasHand) return;

  const int diceTop = top + LABEL + BEST + 18;
  const uint8_t faces[yz::kDice] = {
      static_cast<uint8_t>(model.lastFace), static_cast<uint8_t>(model.lastFace), static_cast<uint8_t>(model.lastFace),
      static_cast<uint8_t>(model.lastFace), static_cast<uint8_t>(model.lastFace)};
  drawDiceRow(frame, diceTop, faces, 0);
  char caption[48];
  snprintf(caption, sizeof(caption), model.fives == 1 ? "FIVE OF A KIND ROLLED ONCE" : "FIVE OF A KIND ROLLED %d TIMES",
           model.fives);
  drawTextInBox(frame, Font::Size14, SIDE, diceTop + DIE + 12, BODY_WIDTH, CAPTION, caption);
}

MenuTap menuTapAt(const MenuModel& model, int x, int y) {
  const MenuRows rows = menuRows(model);
  for (int i = 0; i < rows.count; i++) {
    if (ui::isInBox(x, y, ui::MARGIN, rows.topOf(i), SCREEN_WIDTH - 2 * ui::MARGIN, ui::ROW_HEIGHT)) return rows.taps[i];
  }
  return MenuTap::Nothing;
}

// --- How to play ---

namespace {

const char* const LESSONS[] = {
    "ROLL FIVE DICE. TAP THE ONES YOU WANT TO KEEP AND ROLL AGAIN, UP TO THREE ROLLS.",
    "THEN PUT THE HAND IN ONE BOX ON YOUR CARD. EVERY BOX IS USED EXACTLY ONCE.",
    "SIXTY-THREE IN THE TOP HALF EARNS THIRTY-FIVE MORE. THIRTEEN TURNS EACH.",
    "A SECOND FIVE OF A KIND SCORES 100 MORE, AND MAY FORCE THE ONE MARKED BOX.",
};
constexpr int LESSON_COUNT = sizeof(LESSONS) / sizeof(LESSONS[0]);
constexpr int LESSON_TEXT_HEIGHT = 150;

}  // namespace

int howToPages() { return LESSON_COUNT; }

void drawHowTo(uint8_t* frame, int page) {
  if (page < 0) page = 0;
  if (page >= LESSON_COUNT) page = LESSON_COUNT - 1;
  char progress[16];
  snprintf(progress, sizeof(progress), "%d OF %d", page + 1, LESSON_COUNT);
  ui::drawBand(frame, "HOW TO PLAY", progress, true, Font::Size20);
  ui::drawPill(frame, SIDE, PILL_TOP, BODY_WIDTH, page + 1 < LESSON_COUNT ? "NEXT" : "GOT IT", true);

  const int textHeight = drawWrapped(frame, Font::Size20, SIDE, ui::CONTENT_TOP, BODY_WIDTH, LESSONS[page], Align::Center);
  const int bandTop = ui::CONTENT_TOP + (textHeight > LESSON_TEXT_HEIGHT ? textHeight : LESSON_TEXT_HEIGHT) + 10;
  const int room = PILL_TOP - 12 - bandTop;

  if (page < 3) {
    // Real dice at the real size: a hand being kept, the hand it becomes, then a top
    // half run worth the bonus.
    static const uint8_t FACES[3][yz::kDice] = {{5, 5, 2, 5, 3}, {5, 5, 5, 5, 1}, {4, 4, 4, 4, 4}};
    static const uint8_t HELD[3] = {0b01011, 0b01111, 0b11111};
    drawDiceRow(frame, bandTop + (room - DIE) / 2, FACES[page], HELD[page]);
    return;
  }

  // What the bottom half pays.
  static const char* const NAMES[7] = {"THREE OF A KIND", "FOUR OF A KIND", "FULL HOUSE",   "SMALL STRAIGHT",
                                       "LARGE STRAIGHT",  "FIVE OF A KIND", "CHANCE"};
  static const char* const VALUES[7] = {"ALL FIVE DICE", "ALL FIVE DICE", "25", "30", "40", "50", "ALL FIVE DICE"};
  constexpr int ROW = 34;
  const int top = bandTop + (room > 7 * ROW ? (room - 7 * ROW) / 2 : 0);
  for (int i = 0; i < 7; ++i) {
    const int rowTop = top + i * ROW;
    if (i > 0) fillRect(frame, SIDE, rowTop, BODY_WIDTH, ui::HAIRLINE, true);
    ui::drawTextLeft(frame, Font::Size14, SIDE, rowTop, ROW, NAMES[i]);
    drawRight(frame, Font::Size14, SIDE + BODY_WIDTH, rowTop, ROW, VALUES[i]);
  }
}

bool isOnHowToNext(int x, int y) { return ui::isInBox(x, y, SIDE, PILL_TOP, BODY_WIDTH, ui::PILL_HEIGHT); }

// --- The result ---

void drawResult(uint8_t* frame, const ResultModel& model) {
  const int mine = yz::total(*model.yours);
  const int theirs = yz::total(*model.theirs);
  ui::drawBand(frame, mine == theirs ? "A TIE" : (mine > theirs ? "YOU WIN" : "THEY WIN"), nullptr, true);

  char line[24];
  snprintf(line, sizeof(line), "%d - %d", mine, theirs);
  drawTextInBox(frame, Font::Size44, SIDE, ui::CONTENT_TOP + 20, BODY_WIDTH, 80, line);

  // Where it was won: the three boxes with the widest gap, either way, and the top
  // half's bonus, which no box shows.
  constexpr int ROW = 34;
  int top = ui::CONTENT_TOP + 140;
  ui::drawTextLeft(frame, Font::Size14, SIDE, top, ROW, "BIGGEST SWINGS");
  top += ROW + 4;
  bool used[yz::kCategories] = {};
  for (int shown = 0; shown < 3; ++shown) {
    int bestIndex = -1;
    int bestGap = 0;
    for (int i = 0; i < yz::kCategories; ++i) {
      if (used[i]) continue;
      const int gap = model.yours->box[i] - model.theirs->box[i];
      const int size = gap < 0 ? -gap : gap;
      if (bestIndex < 0 || size > bestGap) {
        bestIndex = i;
        bestGap = size;
      }
    }
    if (bestIndex < 0 || bestGap == 0) break;
    used[bestIndex] = true;
    fillRect(frame, SIDE, top, BODY_WIDTH, ui::HAIRLINE, true);
    ui::drawTextLeft(frame, Font::Size14, SIDE, top, ROW, categoryName(bestIndex));
    char scores[16];
    snprintf(scores, sizeof(scores), "%d - %d", model.yours->box[bestIndex], model.theirs->box[bestIndex]);
    drawRight(frame, Font::Size14, SIDE + BODY_WIDTH, top, ROW, scores);
    top += ROW;
  }
  if (yz::bonusEarned(*model.yours) != yz::bonusEarned(*model.theirs)) {
    fillRect(frame, SIDE, top, BODY_WIDTH, ui::HAIRLINE, true);
    ui::drawTextLeft(frame, Font::Size14, SIDE, top, ROW, "TOP HALF BONUS");
    char bonus[16];
    snprintf(bonus, sizeof(bonus), "%d - %d", yz::bonusEarned(*model.yours), yz::bonusEarned(*model.theirs));
    drawRight(frame, Font::Size14, SIDE + BODY_WIDTH, top, ROW, bonus);
  }

  ui::drawPill(frame, SIDE, PILL_TOP, HALF_WIDTH, "DONE", false);
  ui::drawPill(frame, SIDE + HALF_WIDTH + ui::GAP, PILL_TOP, HALF_WIDTH, "PLAY AGAIN", true);
}

ResultTap resultTapAt(int x, int y) {
  if (ui::isInBox(x, y, SIDE, PILL_TOP, HALF_WIDTH, ui::PILL_HEIGHT)) return ResultTap::Done;
  if (ui::isInBox(x, y, SIDE + HALF_WIDTH + ui::GAP, PILL_TOP, HALF_WIDTH, ui::PILL_HEIGHT)) return ResultTap::Again;
  return ResultTap::Nothing;
}

}  // namespace yachtui
