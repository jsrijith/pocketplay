#include "sudoku_screens.h"

#include <cstdio>

#include "core/graphics.h"
#include "core/text.h"
#include "ui/ui.h"

namespace sudokuui {

namespace {

namespace sk = sudoku;

// The board: 50 px cells inside a 9 px frame, 468 wide, under the warning line.
constexpr int GRID_SIDE = CELL * sk::kSize;
constexpr int BOX_RULE = 5;
constexpr int BOARD_OUTER = GRID_SIDE + 2 * ui::BOARD_FRAME;
constexpr int BOARD_LEFT = (SCREEN_WIDTH - BOARD_OUTER) / 2;
constexpr int BOARD_TOP = ui::CONTENT_TOP;
constexpr int GRID_LEFT = BOARD_LEFT + ui::BOARD_FRAME;
constexpr int GRID_TOP = BOARD_TOP + ui::BOARD_FRAME;

// The pad ends on the bottom margin, and the rail beside it is exactly as tall.
// CrossPlay's keys are 69 px; PocketPlay's grid starts lower, so they are 64 here.
constexpr int SIDE = 16;
constexpr int BOTTOM = SCREEN_HEIGHT - SIDE;
constexpr int PAD_KEY = 64;
constexpr int PAD_SIDE = 3 * PAD_KEY;
constexpr int PAD_TOP = BOTTOM - PAD_SIDE;
constexpr int RAIL_GAP = PAD_TOP - (BOARD_TOP + BOARD_OUTER);
constexpr int RAIL_ROW = 60;
constexpr int RAIL_ROW_GAP = (PAD_SIDE - 3 * RAIL_ROW) / 2;
constexpr int RAIL_LEFT = BOARD_LEFT + PAD_SIDE + RAIL_GAP;
constexpr int RAIL_WIDTH = BOARD_LEFT + BOARD_OUTER - RAIL_LEFT;
static_assert(RAIL_GAP >= 12, "the pad needs air under the grid");
static_assert(3 * RAIL_ROW + 2 * RAIL_ROW_GAP == PAD_SIDE, "the rail is exactly as tall as the pad");

constexpr int BODY_WIDTH = SCREEN_WIDTH - 2 * SIDE;
constexpr int PILL_TOP = BOTTOM - ui::PILL_HEIGHT;
constexpr int GUTTER = 12;

int railTop(int row) { return PAD_TOP + row * (RAIL_ROW + RAIL_ROW_GAP); }

void lightGrey(uint8_t* frame, int left, int top, int width, int height) {
  for (int y = top; y < top + height; y++) {
    for (int x = left; x < left + width; x++) {
      if (x % 2 == 0 && y % 2 == 0) setPixel(frame, x, y, true);
    }
  }
}

// CrossPlay's dark grey: every other pixel.
void darkGrey(uint8_t* frame, int left, int top, int width, int height) {
  for (int y = top; y < top + height; y++) {
    for (int x = left; x < left + width; x++) setPixel(frame, x, y, (x + y) % 2 == 0);
  }
}

void frameBox(uint8_t* frame, int left, int top, int width, int height, int weight, bool white = false) {
  fillRect(frame, left, top, width, weight, !white);
  fillRect(frame, left, top + height - weight, width, weight, !white);
  fillRect(frame, left, top, weight, height, !white);
  fillRect(frame, left + width - weight, top, weight, height, !white);
}

// Brackets at the four corners: the mark on every copy of the armed digit.
void cornerMarks(uint8_t* frame, int left, int top, int width, int height, int arm, int weight, bool white) {
  const int right = left + width;
  const int bottom = top + height;
  fillRect(frame, left, top, arm, weight, !white);
  fillRect(frame, left, top, weight, arm, !white);
  fillRect(frame, right - arm, top, arm, weight, !white);
  fillRect(frame, right - weight, top, weight, arm, !white);
  fillRect(frame, left, bottom - weight, arm, weight, !white);
  fillRect(frame, left, bottom - arm, weight, arm, !white);
  fillRect(frame, right - arm, bottom - weight, arm, weight, !white);
  fillRect(frame, right - weight, bottom - arm, weight, arm, !white);
}

void digitIn(uint8_t* frame, Font font, int left, int top, int width, int height, int digit, bool black = true) {
  const char text[2] = {static_cast<char>('0' + digit), '\0'};
  drawTextInBox(frame, font, left, top, width, height, text, black);
}

// Pencil marks in a three by three of their own, where a phone keypad puts them.
void drawNotes(uint8_t* frame, int left, int top, int size, sk::Mask notes) {
  if (notes == 0) return;
  constexpr int INSET = 4;
  const int side = (size - 2 * INSET) / 3;
  for (int digit = 1; digit <= sk::kSize; ++digit) {
    if (!(notes & sk::bitFor(digit))) continue;
    digitIn(frame, Font::Size10, left + INSET + (digit - 1) % 3 * side, top + INSET + (digit - 1) / 3 * side, side, side, digit);
  }
}

// A rail button: outlined, grey when it can't act, solid when it's the way on.
void drawRailButton(uint8_t* frame, int row, const char* label, bool enabled, bool solid = false) {
  const int top = railTop(row);
  if (solid) {
    fillRect(frame, RAIL_LEFT, top, RAIL_WIDTH, RAIL_ROW, true);
  } else if (!enabled) {
    lightGrey(frame, RAIL_LEFT, top, RAIL_WIDTH, RAIL_ROW);
  }
  if (!solid) drawRectOutline(frame, RAIL_LEFT, top, RAIL_WIDTH, RAIL_ROW, ui::HAIRLINE);
  drawTextInBox(frame, Font::Size20, RAIL_LEFT, top, RAIL_WIDTH, RAIL_ROW, label, !solid);
}

// The front door's grid in miniature: clues solid, the player's digits as smaller
// squares, inside corner brackets.
void drawMiniature(uint8_t* frame, int roomLeft, int roomTop, int roomWidth, int roomHeight, const sk::Game* game, bool hasGame) {
  const int reach = SIDE - 4;
  const int across = roomWidth - 2 * reach;
  const int down = roomHeight - 2 * reach;
  const int side = across < down ? across : down;
  const int cell = side / sk::kSize;
  const int grid = cell * sk::kSize;
  const int left = roomLeft + (roomWidth - grid) / 2;
  const int top = roomTop + (roomHeight - grid) / 2;
  cornerMarks(frame, left - reach, top - reach, grid + 2 * reach, grid + 2 * reach, 16, ui::RULE, false);
  for (int index = 0; index <= sk::kSize; index += sk::kBoxSize) {
    const int offset = index * cell - (index == sk::kSize ? ui::HAIRLINE : 0);
    for (int i = 0; i < grid; i++) {
      if ((left + offset + top + i) % 2 == 0) setPixel(frame, left + offset, top + i, true);
      if ((left + i + top + offset) % 2 == 0) setPixel(frame, left + i, top + offset, true);
    }
  }
  if (!hasGame || game == nullptr) return;
  for (int index = 0; index < sk::kCells; ++index) {
    const int x = left + (index % sk::kSize) * cell;
    const int y = top + (index / sk::kSize) * cell;
    if (sk::isGiven(*game, index)) {
      fillRect(frame, x + 2, y + 2, cell - 4, cell - 4, true);
    } else if (game->entry[index] != 0) {
      const int pad = cell / 3;
      fillRect(frame, x + pad, y + pad, cell - 2 * pad, cell - 2 * pad, true);
    }
  }
}

// --- The menu's rows, anchored to the bottom ---

int menuRowTop(int row) { return BOTTOM - ui::ROW_HEIGHT - (1 - row) * (ui::ROW_HEIGHT + ui::ROW_GAP); }
constexpr int MENU_PILL_GAP = GUTTER;
int menuPillTop() { return menuRowTop(0) - MENU_PILL_GAP - ui::PILL_HEIGHT; }

void drawMenuRow(uint8_t* frame, int row, const char* label, const char* value, bool selected) {
  const int top = menuRowTop(row);
  const int width = SCREEN_WIDTH - 2 * ui::MARGIN;
  if (selected) {
    fillRect(frame, ui::MARGIN, top, width, ui::ROW_HEIGHT, true);
  } else {
    drawRectOutline(frame, ui::MARGIN, top, width, ui::ROW_HEIGHT, ui::HAIRLINE);
  }
  ui::drawTextLeft(frame, Font::Size20, ui::MARGIN + 14, top, ui::ROW_HEIGHT, label, !selected);
  if (value != nullptr) {
    const int cap = capHeight(Font::Size20);
    drawText(frame, Font::Size20, ui::MARGIN + width - 14, top + (ui::ROW_HEIGHT - cap) / 2 + cap, value, Align::Right, !selected);
  }
}

// --- How to play ---

struct Lesson {
  const char* title;
  const char* body;
  const char* detail;
  const char* before;
  const char* after;  // nullptr when the page is a rule rather than a gesture
  uint8_t armed;
};

// CrossPlay's five, each gesture shown as a before and after.
const Lesson LESSONS[] = {
    {"THE RULE", "EVERY ROW, COLUMN AND BOX HOLDS 1 TO 9.", "NO DIGIT TWICE IN ANY OF THEM.", "123456789", nullptr, 0},
    {"WRITING", "PICK A DIGIT, THEN TAP A CELL TO WRITE IT.", "EVERY COPY OF IT IS MARKED.", "A.C.....I", "A.C.5...I", 5},
    {"CLEARING", "TAP YOUR OWN DIGIT AGAIN TO CLEAR IT.", "TAP A CLUE TO PICK ITS DIGIT UP.", "A.C.5...I", "A.C.....I", 5},
    {"PENCIL", "HOLD A CELL TO PENCIL A DIGIT IN.", "HOLD AGAIN TO RUB IT OUT.", "A.C.....I", "A.C.p...I", 0},
    {"MISTAKES", "A DIGIT THAT CLASHES TURNS BLACK.", "HINT NAMES A CELL YOU CAN SOLVE.", "A.C.5...I", "A.C!!...I", 5},
};
constexpr int LESSON_COUNT = sizeof(LESSONS) / sizeof(LESSONS[0]);

// A three by three from a string: 'A'-'I' clues 1-9, '1'-'9' the player's digits,
// '!' a clash of the armed digit, 'p' pencil marks, '.' empty.
void drawFace(uint8_t* frame, int boxLeft, int boxTop, int boxSide, const char* face, uint8_t armed) {
  const int cell = boxSide / 3;
  const int grid = cell * 3;
  const int left = boxLeft + (boxSide - grid) / 2;
  const int top = boxTop + (boxSide - grid) / 2;
  const Font font = cell >= 64 ? Font::Size30 : Font::Size20;
  for (int index = 0; index < 9; ++index) {
    const int x = left + (index % 3) * cell;
    const int y = top + (index / 3) * cell;
    const char mark = face[index];
    const bool clue = mark >= 'A' && mark <= 'I';
    const bool clash = mark == '!';
    if (clash) {
      fillRect(frame, x, y, cell, cell, true);
    } else if (clue) {
      lightGrey(frame, x, y, cell, cell);
    }
    frameBox(frame, x, y, cell, cell, ui::HAIRLINE);
    int digit = 0;
    if (clue) digit = 1 + (mark - 'A');
    if (mark >= '1' && mark <= '9') digit = mark - '0';
    if (clash) digit = armed;
    if (mark == 'p') {
      drawNotes(frame, x, y, cell, static_cast<sk::Mask>(sk::bitFor(2) | sk::bitFor(6) | sk::bitFor(9)));
    } else if (digit != 0) {
      digitIn(frame, font, x, y, cell, cell, digit, !clash);
      if (armed != 0 && digit == armed) cornerMarks(frame, x, y, cell, cell, cell / 4, ui::RULE, clash);
    }
  }
  frameBox(frame, left - ui::RULE, top - ui::RULE, grid + 2 * ui::RULE, grid + 2 * ui::RULE, ui::RULE);
}

void drawArrow(uint8_t* frame, int top, int height) {
  const int midX = SCREEN_WIDTH / 2;
  const int midY = top + height / 2;
  const int shaft = height / 2;
  fillRect(frame, midX - 2, midY - shaft, 5, shaft, true);
  for (int i = 0; i < 9; ++i) fillRect(frame, midX - (9 - i), midY + i, (9 - i) * 2, 1, true);
}

// --- The result ---

constexpr int GRID_BUTTON_TOP = PILL_TOP - GUTTER - ui::PILL_HEIGHT;

}  // namespace

void formatClock(uint32_t ms, char* out, int size) {
  const uint32_t seconds = ms / 1000;
  const uint32_t hours = seconds / 3600;
  if (hours > 0) {
    snprintf(out, static_cast<size_t>(size), "%u:%02u:%02u", static_cast<unsigned>(hours), static_cast<unsigned>((seconds / 60) % 60),
             static_cast<unsigned>(seconds % 60));
    return;
  }
  snprintf(out, static_cast<size_t>(size), "%u:%02u", static_cast<unsigned>(seconds / 60), static_cast<unsigned>(seconds % 60));
}

// --- The board ---

void cellOrigin(int cell, int& left, int& top) {
  left = GRID_LEFT + (cell % sk::kSize) * CELL;
  top = GRID_TOP + (cell / sk::kSize) * CELL;
}

bool cellAt(int x, int y, int& cell) {
  if (x < GRID_LEFT || y < GRID_TOP) return false;
  const int column = (x - GRID_LEFT) / CELL;
  const int row = (y - GRID_TOP) / CELL;
  if (column >= sk::kSize || row >= sk::kSize) return false;
  cell = row * sk::kSize + column;
  return true;
}

bool padKeyAt(int x, int y, int& digit) {
  if (x < BOARD_LEFT || y < PAD_TOP) return false;
  const int column = (x - BOARD_LEFT) / PAD_KEY;
  const int row = (y - PAD_TOP) / PAD_KEY;
  if (column >= 3 || row >= 3) return false;
  digit = row * 3 + column + 1;
  return true;
}

RailTap railTapAt(int x, int y) {
  const RailTap rows[3] = {RailTap::Status, RailTap::Undo, RailTap::Hint};
  for (int row = 0; row < 3; row++) {
    if (ui::isInBox(x, y, RAIL_LEFT, railTop(row), RAIL_WIDTH, RAIL_ROW)) return rows[row];
  }
  return RailTap::Nothing;
}

void drawBoard(uint8_t* frame, const BoardModel& model) {
  const sk::Game& game = *model.game;
  ui::drawBand(frame, "SUDOKU", sk::levelName(game.puzzle.level), true, Font::Size20);

  fillRect(frame, BOARD_LEFT, BOARD_TOP, BOARD_OUTER, BOARD_OUTER, true);
  fillRect(frame, GRID_LEFT, GRID_TOP, GRID_SIDE, GRID_SIDE, false);
  if (!model.generating) {
    for (int cell = 0; cell < sk::kCells; ++cell) {
      int x = 0;
      int y = 0;
      cellOrigin(cell, x, y);
      const uint8_t value = sk::valueAt(game, cell);
      const bool clash = sk::isClashing(game, cell);
      if (clash) {
        fillRect(frame, x, y, CELL, CELL, true);
      } else if (cell == model.holdCell) {
        darkGrey(frame, x, y, CELL, CELL);
      } else if (sk::isGiven(game, cell)) {
        lightGrey(frame, x, y, CELL, CELL);
      }
      if (value != 0) {
        digitIn(frame, Font::Size30, x, y, CELL, CELL, value, !clash);
        if (value == game.armed) cornerMarks(frame, x, y, CELL, CELL, 13, ui::RULE, clash);
      } else {
        drawNotes(frame, x, y, CELL, sk::visibleNotes(game, cell));
      }
      if (cell == game.hintCell) frameBox(frame, x, y, CELL, CELL, ui::FRAME);
    }
  }
  for (int index = 1; index < sk::kSize; ++index) {
    const int weight = index % sk::kBoxSize == 0 ? BOX_RULE : ui::HAIRLINE;
    const int offset = index * CELL - weight / 2;
    fillRect(frame, GRID_LEFT + offset, GRID_TOP, weight, GRID_SIDE, true);
    fillRect(frame, GRID_LEFT, GRID_TOP + offset, GRID_SIDE, weight, true);
  }

  for (int digit = 1; digit <= sk::kSize; ++digit) {
    const int x = BOARD_LEFT + (digit - 1) % 3 * PAD_KEY;
    const int y = PAD_TOP + (digit - 1) / 3 * PAD_KEY;
    if (!model.generating && sk::placedCount(game, digit) >= sk::kSize) lightGrey(frame, x, y, PAD_KEY, PAD_KEY);
    frameBox(frame, x, y, PAD_KEY, PAD_KEY, digit == game.armed ? ui::FRAME : ui::HAIRLINE);
    digitIn(frame, Font::Size30, x, y, PAD_KEY, PAD_KEY, digit);
  }

  const bool solved = game.solvedFlag != 0;
  char line[32];
  if (model.generating) {
    snprintf(line, sizeof(line), "MAKING ONE");
  } else if (solved) {
    snprintf(line, sizeof(line), "SOLVED");
  } else if (model.notice != nullptr) {
    snprintf(line, sizeof(line), "%s", model.notice);
  } else {
    snprintf(line, sizeof(line), "%d LEFT", sk::emptyCount(game));
  }
  drawRailButton(frame, 0, line, true, solved && !model.generating);
  drawRailButton(frame, 1, "UNDO", sk::canUndo(game) && !model.generating);
  drawRailButton(frame, 2, "HINT", !solved && !model.generating);
}

// --- The menu ---

void drawMenu(uint8_t* frame, const MenuModel& model) {
  ui::drawBand(frame, "SUDOKU", sk::levelName(model.level), true, Font::Size20);
  const sk::Game& game = *model.game;
  const sk::Record& record = *model.record;

  const sk::MenuOffer offer = sk::menuOffer(game, model.hasGame, model.level);
  char state[48];
  switch (offer) {
    case sk::MenuOffer::OtherLevel:
      snprintf(state, sizeof(state), "%s, STARTING FRESH", sk::levelName(model.level));
      break;
    case sk::MenuOffer::Resume:
      snprintf(state, sizeof(state), "%d LEFT", sk::emptyCount(game));
      break;
    case sk::MenuOffer::Solved:
      snprintf(state, sizeof(state), "LAST ONE SOLVED");
      break;
    case sk::MenuOffer::Fresh:
      snprintf(state, sizeof(state), "NOT STARTED");
      break;
  }
  constexpr int STATE_HEIGHT = 34;
  ui::drawTextLeft(frame, Font::Size20, SIDE, ui::CONTENT_TOP, STATE_HEIGHT, state);

  char line[56];
  const int index = static_cast<int>(model.level);
  if (record.bestMs[index] != 0) {
    char best[16];
    formatClock(record.bestMs[index], best, sizeof(best));
    snprintf(line, sizeof(line), "%d SOLVED   BEST %s", sk::totalSolved(record), best);
  } else {
    snprintf(line, sizeof(line), "%d SOLVED   NO %s TIME YET", sk::totalSolved(record), sk::levelName(model.level));
  }
  const int recordTop = ui::CONTENT_TOP + STATE_HEIGHT;
  ui::drawTextLeft(frame, Font::Size10, SIDE, recordTop, lineHeight(Font::Size10), line);

  ui::drawPill(frame, SIDE, menuPillTop(), BODY_WIDTH, offer == sk::MenuOffer::Resume ? "RESUME" : "NEW PUZZLE", true);
  drawMenuRow(frame, 0, "DIFFICULTY", sk::levelName(model.level), model.levelSelected);
  drawMenuRow(frame, 1, "HOW TO PLAY", nullptr, false);

  const int top = recordTop + lineHeight(Font::Size10) + GUTTER;
  drawMiniature(frame, SIDE, top, BODY_WIDTH, menuPillTop() - GUTTER - top, model.game, model.hasGame);
}

MenuTap menuTapAt(int x, int y) {
  if (ui::isInBox(x, y, SIDE, menuPillTop(), BODY_WIDTH, ui::PILL_HEIGHT)) return MenuTap::Play;
  const int width = SCREEN_WIDTH - 2 * ui::MARGIN;
  if (ui::isInBox(x, y, ui::MARGIN, menuRowTop(0), width, ui::ROW_HEIGHT)) return MenuTap::Level;
  if (ui::isInBox(x, y, ui::MARGIN, menuRowTop(1), width, ui::ROW_HEIGHT)) return MenuTap::HowTo;
  return MenuTap::Nothing;
}

// --- How to play ---

int howToPages() { return LESSON_COUNT; }

void drawHowTo(uint8_t* frame, int page) {
  if (page < 0) page = 0;
  if (page >= LESSON_COUNT) page = LESSON_COUNT - 1;
  const Lesson& lesson = LESSONS[page];
  char progress[16];
  snprintf(progress, sizeof(progress), "%d OF %d", page + 1, LESSON_COUNT);
  ui::drawBand(frame, "HOW TO PLAY", progress, true, Font::Size20);
  ui::drawPill(frame, SIDE, PILL_TOP, BODY_WIDTH, page + 1 < LESSON_COUNT ? "NEXT" : "GOT IT", true);

  int top = ui::CONTENT_TOP;
  constexpr int TITLE_HEIGHT = 44;
  drawTextInBox(frame, Font::Size30, SIDE, top, BODY_WIDTH, TITLE_HEIGHT, lesson.title);
  top += TITLE_HEIGHT;
  top += drawWrapped(frame, Font::Size20, SIDE, top, BODY_WIDTH, lesson.body, Align::Center);
  top += GUTTER;

  const int detailTop = PILL_TOP - GUTTER - lineHeight(Font::Size10);
  drawTextInBox(frame, Font::Size10, SIDE, detailTop, BODY_WIDTH, lineHeight(Font::Size10), lesson.detail);

  const int roomHeight = detailTop - GUTTER - top;
  if (lesson.after == nullptr) {
    const int side = BODY_WIDTH < roomHeight ? BODY_WIDTH : roomHeight;
    drawFace(frame, SIDE + (BODY_WIDTH - side) / 2, top + (roomHeight - side) / 2, side, lesson.before, lesson.armed);
    return;
  }
  constexpr int ARROW = 54;
  const int side = (roomHeight - ARROW) / 2 < BODY_WIDTH ? (roomHeight - ARROW) / 2 : BODY_WIDTH;
  const int faceTop = top + (roomHeight - (side * 2 + ARROW)) / 2;
  const int left = SIDE + (BODY_WIDTH - side) / 2;
  drawFace(frame, left, faceTop, side, lesson.before, lesson.armed);
  drawArrow(frame, faceTop + side, ARROW);
  drawFace(frame, left, faceTop + side + ARROW, side, lesson.after, lesson.armed);
}

bool isOnHowToNext(int x, int y) { return ui::isInBox(x, y, SIDE, PILL_TOP, BODY_WIDTH, ui::PILL_HEIGHT); }

// --- The result ---

void drawResult(uint8_t* frame, const ResultModel& model) {
  const sk::Game& game = *model.game;
  const sk::Level level = game.puzzle.level;
  ui::drawBand(frame, "SOLVED", sk::levelName(level), true, Font::Size20);

  int top = ui::CONTENT_TOP;
  char clock[16];
  formatClock(game.elapsedMs, clock, sizeof(clock));
  ui::drawTextLeft(frame, Font::Size30, SIDE, top, 72, clock);
  top += 72;

  char under[48];
  if (game.hintsUsed > 0) {
    snprintf(under, sizeof(under), "%d HINT%s, SO NO TIME RECORDED", game.hintsUsed, game.hintsUsed == 1 ? "" : "S");
  } else if (model.newBest) {
    snprintf(under, sizeof(under), "YOUR BEST %s YET", sk::levelName(level));
  } else if (model.bestMs != 0) {
    char best[16];
    formatClock(model.bestMs, best, sizeof(best));
    snprintf(under, sizeof(under), "YOUR BEST IS %s", best);
  } else {
    snprintf(under, sizeof(under), "UNAIDED");
  }
  ui::drawTextLeft(frame, Font::Size20, SIDE, top, 34, under);
  top += 34;
  fillRect(frame, SIDE, top + 8, BODY_WIDTH, ui::RULE, true);
  top += ui::RULE + 12;

  char detail[72];
  snprintf(detail, sizeof(detail), "%d CLUES   HARDEST %s   %d %s SOLVED", game.puzzle.clues, sk::techniqueName(game.puzzle.hardest),
           model.solvedAtThisLevel, sk::levelName(level));
  ui::drawTextLeft(frame, Font::Size10, SIDE, top, 26, detail);
  top += 26;

  ui::drawPill(frame, SIDE, PILL_TOP, BODY_WIDTH, "ANOTHER", true);
  ui::drawPill(frame, SIDE, GRID_BUTTON_TOP, BODY_WIDTH, "BACK TO THE GRID", false);
  drawMiniature(frame, SIDE, top, BODY_WIDTH, GRID_BUTTON_TOP - GUTTER - top, model.game, true);
}

ResultTap resultTapAt(int x, int y) {
  if (ui::isInBox(x, y, SIDE, PILL_TOP, BODY_WIDTH, ui::PILL_HEIGHT)) return ResultTap::Another;
  if (ui::isInBox(x, y, SIDE, GRID_BUTTON_TOP, BODY_WIDTH, ui::PILL_HEIGHT)) return ResultTap::Grid;
  return ResultTap::Nothing;
}

}  // namespace sudokuui
