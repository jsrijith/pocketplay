#include "knucklebones_screens.h"

#include <cstdio>

#include "core/graphics.h"
#include "core/text.h"
#include "ui/ui.h"

namespace knuckleui {

namespace {

namespace kb = knucklebones;

constexpr int SIDE = 16;
constexpr int BODY_WIDTH = SCREEN_WIDTH - 2 * SIDE;
constexpr int BOTTOM = SCREEN_HEIGHT - 16;
constexpr int PILL_TOP = BOTTOM - ui::PILL_HEIGHT;

// The board, centred, as CrossPlay's ladder: their grid, their column scores, the
// die between the two totals, your grid, your column scores, then the status
// capsule. Each step follows from the one above. PocketPlay's warning line starts
// the ladder 22 px lower than CrossPlay's chrome does, so the cells are 70 px
// rather than 73, and the gaps are CrossPlay's.
constexpr int CELL = 70;
constexpr int CELL_GAP = 6;
constexpr int GRID_SIDE = CELL * kb::kColumns + CELL_GAP * (kb::kColumns - 1);
constexpr int GRID_LEFT = (SCREEN_WIDTH - GRID_SIDE) / 2;
constexpr int SCORE_BAND = 28;
constexpr int SCORE_GAP = 4;
// Their dithered ground is outset 4 px, so the grid starts that far below the content.
constexpr int GROUND_OUTSET = 4;
constexpr int THEIRS_TOP = ui::CONTENT_TOP + GROUND_OUTSET;
constexpr int THEIRS_SCORE_TOP = THEIRS_TOP + GRID_SIDE + SCORE_GAP;
constexpr int DIE_GAP_ABOVE = 18;
constexpr int DIE_GAP_BELOW = 8;
constexpr int DIE_TOP = THEIRS_SCORE_TOP + SCORE_BAND + DIE_GAP_ABOVE;
constexpr int DIE_SIZE = CELL;
constexpr int DIE_LEFT = (SCREEN_WIDTH - DIE_SIZE) / 2;
constexpr int YOURS_TOP = DIE_TOP + DIE_SIZE + DIE_GAP_BELOW;
constexpr int YOURS_SCORE_TOP = YOURS_TOP + GRID_SIDE + SCORE_GAP;
static_assert(YOURS_SCORE_TOP + SCORE_BAND + 8 <= PILL_TOP, "the knucklebones ladder has overrun the status capsule");

constexpr int PIP = 9;
constexpr int MINI_GAP = 4;

void lightGrey(uint8_t* frame, int left, int top, int width, int height) {
  for (int y = top; y < top + height; y++) {
    for (int x = left; x < left + width; x++) {
      if (x % 2 == 0 && y % 2 == 0) setPixel(frame, x, y, true);
    }
  }
}

// A die's pips in a 3x3 lattice, laid out on the box's shorter side, as CrossPlay's.
void drawFace(uint8_t* frame, int left, int top, int width, int height, uint8_t value, bool white) {
  static const int8_t LAYOUTS[7][9] = {
      {0, 0, 0, 0, 0, 0, 0, 0, 0},  // unused: 0 is empty
      {0, 0, 0, 0, 1, 0, 0, 0, 0}, {1, 0, 0, 0, 0, 0, 0, 0, 1}, {1, 0, 0, 0, 1, 0, 0, 0, 1},
      {1, 0, 1, 0, 0, 0, 1, 0, 1}, {1, 0, 1, 0, 1, 0, 1, 0, 1}, {1, 0, 1, 1, 0, 1, 1, 0, 1},
  };
  if (value < 1 || value > kb::kFaces) return;
  const int side = width < height ? width : height;
  const int originX = left + (width - side) / 2;
  const int originY = top + (height - side) / 2;
  const int step = side / 4;
  for (int i = 0; i < 9; ++i) {
    if (LAYOUTS[value][i] == 0) continue;
    const int cx = originX + step * (i % 3 + 1);
    const int cy = originY + step * (i / 3 + 1);
    fillRect(frame, cx - PIP / 2, cy - PIP / 2, PIP, PIP, !white);
  }
}

// Row 0 is the near end for whoever owns the grid: yours stacks up from the bottom
// and theirs down from the top, so the two face each other across the die.
void cellOrigin(int column, int row, bool yours, int& left, int& top) {
  left = GRID_LEFT + column * (CELL + CELL_GAP);
  top = yours ? YOURS_TOP + GRID_SIDE - CELL - row * (CELL + CELL_GAP) : THEIRS_TOP + row * (CELL + CELL_GAP);
}

bool matched(const kb::Grid& grid, int column, uint8_t value) {
  int same = 0;
  for (int row = 0; row < kb::kRows; ++row) {
    if (grid.cell[column][row] == value) ++same;
  }
  return same >= 2;
}

void drawGrid(uint8_t* frame, const kb::Grid& grid, bool yours) {
  // Their ground is dithered and yours is paper, so it's clear which is yours.
  if (!yours) lightGrey(frame, GRID_LEFT - GROUND_OUTSET, THEIRS_TOP - GROUND_OUTSET, GRID_SIDE + 2 * GROUND_OUTSET, GRID_SIDE + 2 * GROUND_OUTSET);

  for (int column = 0; column < kb::kColumns; ++column) {
    for (int row = 0; row < kb::kRows; ++row) {
      int x = 0;
      int y = 0;
      cellOrigin(column, row, yours, x, y);
      const uint8_t value = grid.cell[column][row];
      // Dice that multiply are inverted, so the game's one mechanic shows on the board.
      if (value != kb::kEmpty && matched(grid, column, value)) {
        fillRect(frame, x, y, CELL, CELL, true);
        drawFace(frame, x, y, CELL, CELL, value, true);
      } else {
        fillRect(frame, x, y, CELL, CELL, false);
        drawRectOutline(frame, x, y, CELL, CELL, 2);
        drawFace(frame, x, y, CELL, CELL, value, false);
      }
    }
    // A column worth nothing shows nothing: three empty slots already say so.
    const int total = kb::columnScore(grid, column);
    if (total == 0) continue;
    char label[8];
    snprintf(label, sizeof(label), "%d", total);
    drawTextInBox(frame, Font::Size20, GRID_LEFT + column * (CELL + CELL_GAP), yours ? YOURS_SCORE_TOP : THEIRS_SCORE_TOP, CELL, SCORE_BAND, label);
  }
}

// Four corner brackets around a column: where the die can go.
void bracket(uint8_t* frame, int left, int top, int width, int height) {
  constexpr int ARM = 16;
  constexpr int WEIGHT = 4;
  const int right = left + width;
  const int bottom = top + height;
  fillRect(frame, left, top, ARM, WEIGHT, true);
  fillRect(frame, right - ARM, top, ARM, WEIGHT, true);
  fillRect(frame, left, bottom - WEIGHT, ARM, WEIGHT, true);
  fillRect(frame, right - ARM, bottom - WEIGHT, ARM, WEIGHT, true);
  fillRect(frame, left, top, WEIGHT, ARM, true);
  fillRect(frame, right - WEIGHT, top, WEIGHT, ARM, true);
  fillRect(frame, left, bottom - ARM, WEIGHT, ARM, true);
  fillRect(frame, right - WEIGHT, bottom - ARM, WEIGHT, ARM, true);
}

// A grid at any size, row 0 at the bottom as on the real board, for the menu and
// the lessons.
void miniGrid(uint8_t* frame, int left, int top, int cell, const uint8_t cells[kb::kColumns][kb::kRows], bool bracketed) {
  for (int column = 0; column < kb::kColumns; ++column) {
    for (int row = 0; row < kb::kRows; ++row) {
      const int x = left + column * (cell + MINI_GAP);
      const int y = top + (kb::kRows - 1 - row) * (cell + MINI_GAP);
      fillRect(frame, x, y, cell, cell, false);
      drawRectOutline(frame, x, y, cell, cell, 2);
      drawFace(frame, x, y, cell, cell, cells[column][row], false);
    }
    if (bracketed) bracket(frame, left + column * (cell + MINI_GAP), top, cell, cell * kb::kRows + MINI_GAP * (kb::kRows - 1));
  }
}

constexpr int miniSide(int cell) { return cell * kb::kColumns + MINI_GAP * (kb::kColumns - 1); }

// --- The menu's rows, anchored to the bottom ---

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
    rows.count = 2;
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

const char* const LESSONS[] = {
    "TWO BOARDS FACING. ROLL A DIE, DROP IT IN ONE OF YOUR THREE COLUMNS.",
    "MATCHING DICE MULTIPLY. THREE 4s SCORE 36, NOT 12.",
    "YOUR 5 DESTROYS EVERY 5 IN THEIR FACING COLUMN.",
};
constexpr int LESSON_COUNT = sizeof(LESSONS) / sizeof(LESSONS[0]);
constexpr int LESSON_TEXT_HEIGHT = 130;

}  // namespace

// --- The board ---

int columnAt(int x, int y) {
  for (int column = 0; column < kb::kColumns; ++column) {
    if (ui::isInBox(x, y, GRID_LEFT + column * (CELL + CELL_GAP), YOURS_TOP, CELL, GRID_SIDE)) return column;
  }
  return -1;
}

void drawBoard(uint8_t* frame, const BoardModel& model) {
  ui::drawBand(frame, "KNUCKLEBONES", nullptr, true);

  // The status capsule, full width, as CrossPlay's: whose roll it is.
  fillRect(frame, 0, PILL_TOP, SCREEN_WIDTH, ui::PILL_HEIGHT, true);
  drawTextInBox(frame, Font::Size20, 0, PILL_TOP, SCREEN_WIDTH, ui::PILL_HEIGHT, model.yourTurn ? "YOUR ROLL" : "THEIR ROLL", false);

  drawGrid(frame, *model.theirs, false);
  drawGrid(frame, *model.yours, true);

  // The die belongs to neither player, so it's centred on the panel, outlined.
  if (model.die != kb::kEmpty) {
    drawRectOutline(frame, DIE_LEFT, DIE_TOP, DIE_SIZE, DIE_SIZE, 4);
    drawFace(frame, DIE_LEFT, DIE_TOP, DIE_SIZE, DIE_SIZE, model.die, false);
  }

  // The totals flank the die, named: THEM on the left by their grid above, YOU on
  // the right by yours below.
  char theirs[8];
  char mine[8];
  snprintf(theirs, sizeof(theirs), "%d", kb::score(*model.theirs));
  snprintf(mine, sizeof(mine), "%d", kb::score(*model.yours));
  constexpr int RIGHT = SCREEN_WIDTH - SIDE;
  ui::drawTextLeft(frame, Font::Size10, SIDE, DIE_TOP, 20, "THEM");
  ui::drawTextLeft(frame, Font::Size30, SIDE, DIE_TOP + 22, 50, theirs);
  ui::drawTextLeft(frame, Font::Size10, RIGHT - textWidth(Font::Size10, "YOU"), DIE_TOP, 20, "YOU");
  ui::drawTextLeft(frame, Font::Size30, RIGHT - textWidth(Font::Size30, mine), DIE_TOP + 22, 50, mine);

  // Where the die can go, only while it's yours to place.
  if (!model.yourTurn || model.die == kb::kEmpty) return;
  for (int column = 0; column < kb::kColumns; ++column) {
    if (!kb::columnHasRoom(*model.yours, column)) continue;
    bracket(frame, GRID_LEFT + column * (CELL + CELL_GAP), YOURS_TOP, CELL, GRID_SIDE);
  }
}

// --- The menu ---

void drawMenu(uint8_t* frame, const MenuModel& model) {
  ui::drawBand(frame, "KNUCKLEBONES", nullptr, true);

  char record[48];
  snprintf(record, sizeof(record), "%d PLAYED   %d WON", model.wins + model.losses + model.draws, model.wins);
  constexpr int LINE_HEIGHT = 26;
  ui::drawTextLeft(frame, Font::Size10, SIDE, ui::CONTENT_TOP, LINE_HEIGHT, record);
  const int ruleTop = ui::CONTENT_TOP + LINE_HEIGHT + 6;
  fillRect(frame, SIDE, ruleTop, BODY_WIDTH, ui::RULE, true);

  const MenuRows rows = menuRows(model);
  for (int i = 0; i < rows.count; i++) {
    if (i == 0) {
      drawSelectedRow(frame, rows.topOf(i), rows.labels[i]);
    } else {
      ui::drawRow(frame, rows.topOf(i), rows.labels[i], nullptr);
    }
  }
  if (!model.hasHistory || model.lastYours == nullptr || model.lastTheirs == nullptr) return;

  // The last match, stacked as the board stacks it, with the tally where the die sits.
  constexpr int MINI = 44;
  constexpr int GRID_HEIGHT = miniSide(MINI);
  constexpr int TALLY = 24;
  constexpr int STACK = GRID_HEIGHT * 2 + 12 + TALLY + 12;
  const int roomTop = ruleTop + ui::RULE;
  const int room = rows.topOf(0) - 12 - roomTop;
  const int top = roomTop + (room > STACK ? (room - STACK) / 2 : 0);
  const int left = (SCREEN_WIDTH - miniSide(MINI)) / 2;
  miniGrid(frame, left, top, MINI, model.lastTheirs->cell, false);
  char tally[48];
  snprintf(tally, sizeof(tally), "%d W   %d L   %d D", model.wins, model.losses, model.draws);
  drawTextInBox(frame, Font::Size10, SIDE, top + GRID_HEIGHT + 12, BODY_WIDTH, TALLY, tally);
  miniGrid(frame, left, top + GRID_HEIGHT + 12 + TALLY + 12, MINI, model.lastYours->cell, false);
}

MenuTap menuTapAt(const MenuModel& model, int x, int y) {
  const MenuRows rows = menuRows(model);
  for (int i = 0; i < rows.count; i++) {
    if (ui::isInBox(x, y, ui::MARGIN, rows.topOf(i), SCREEN_WIDTH - 2 * ui::MARGIN, ui::ROW_HEIGHT)) return rows.taps[i];
  }
  return MenuTap::Nothing;
}

// --- How to play ---

int howToPages() { return LESSON_COUNT; }

void drawHowTo(uint8_t* frame, int page) {
  if (page < 0) page = 0;
  if (page >= LESSON_COUNT) page = LESSON_COUNT - 1;
  char progress[16];
  snprintf(progress, sizeof(progress), "%d OF %d", page + 1, LESSON_COUNT);
  ui::drawBand(frame, "HOW TO PLAY", progress, true, Font::Size20);

  ui::drawPill(frame, SIDE, PILL_TOP, BODY_WIDTH, page + 1 < LESSON_COUNT ? "NEXT" : "GOT IT", true);

  // One sentence and one picture of the real game per page, as CrossPlay's.
  const int textHeight = drawWrapped(frame, Font::Size20, SIDE, ui::CONTENT_TOP, BODY_WIDTH, LESSONS[page], Align::Center);
  const int diagramTop = ui::CONTENT_TOP + (textHeight > LESSON_TEXT_HEIGHT ? textHeight : LESSON_TEXT_HEIGHT) + 10;
  constexpr uint8_t E = kb::kEmpty;

  if (page == 0) {
    // Two grids facing, yours bracketed, one die between them.
    constexpr int MINI = 52;
    const int left = (SCREEN_WIDTH - miniSide(MINI)) / 2;
    const uint8_t theirs[kb::kColumns][kb::kRows] = {{3, E, E}, {E, E, E}, {6, 2, E}};
    const uint8_t yours[kb::kColumns][kb::kRows] = {{E, E, E}, {5, E, E}, {E, E, E}};
    miniGrid(frame, left, diagramTop, MINI, theirs, false);
    const int dieTop = diagramTop + 3 * MINI + 8 + 14;
    drawRectOutline(frame, (SCREEN_WIDTH - MINI) / 2, dieTop, MINI, MINI, 4);
    drawFace(frame, (SCREEN_WIDTH - MINI) / 2, dieTop, MINI, MINI, 4, false);
    miniGrid(frame, left, diagramTop + 3 * MINI + 8 + MINI + 28, MINI, yours, true);
    return;
  }

  if (page == 1) {
    // Three 4s stacked against the same three spread, each score beside its grid.
    constexpr int MINI = 58;
    const uint8_t stacked[kb::kColumns][kb::kRows] = {{4, 4, 4}, {E, E, E}, {E, E, E}};
    const uint8_t spread[kb::kColumns][kb::kRows] = {{4, E, E}, {4, E, E}, {4, E, E}};
    const int width = miniSide(MINI);
    const int left = (SCREEN_WIDTH - width) / 2;
    const int gridHeight = miniSide(MINI);
    const int zoneLeft = left + width + ui::GAP;
    const int zoneWidth = SCREEN_WIDTH - SIDE - zoneLeft;
    miniGrid(frame, left, diagramTop, MINI, stacked, false);
    drawTextInBox(frame, Font::Size30, zoneLeft, diagramTop + (gridHeight - 44) / 2, zoneWidth, 44, "36");
    const int secondTop = diagramTop + gridHeight + 40;
    miniGrid(frame, left, secondTop, MINI, spread, false);
    drawTextInBox(frame, Font::Size30, zoneLeft, secondTop + (gridHeight - 44) / 2, zoneWidth, 44, "12");
    return;
  }

  // Their pair of 5s above, your 5 below in the same column.
  constexpr int MINI = 58;
  const uint8_t theirs[kb::kColumns][kb::kRows] = {{5, 5, E}, {E, E, E}, {E, E, E}};
  const uint8_t yours[kb::kColumns][kb::kRows] = {{5, E, E}, {E, E, E}, {E, E, E}};
  const int left = (SCREEN_WIDTH - miniSide(MINI)) / 2;
  miniGrid(frame, left, diagramTop, MINI, theirs, false);
  drawTextInBox(frame, Font::Size30, SIDE, diagramTop + 3 * MINI + 10, BODY_WIDTH, 44, "^");
  miniGrid(frame, left, diagramTop + 3 * MINI + 60, MINI, yours, true);
}

bool isOnHowToNext(int x, int y) { return ui::isInBox(x, y, SIDE, PILL_TOP, BODY_WIDTH, ui::PILL_HEIGHT); }

// --- The result ---

namespace {
constexpr int DONE_TOP = BOTTOM - ui::ROW_HEIGHT;
constexpr int AGAIN_TOP = DONE_TOP - ui::GAP - ui::ROW_HEIGHT;
}  // namespace

void drawResult(uint8_t* frame, const ResultModel& model) {
  const bool drew = model.yourScore == model.theirScore;
  const bool won = model.yourScore > model.theirScore;
  ui::drawBand(frame, drew ? "A DRAW" : (won ? "YOU WIN" : "THEY WIN"), nullptr, true);

  char line[24];
  snprintf(line, sizeof(line), "%d - %d", model.yourScore, model.theirScore);
  drawTextInBox(frame, Font::Size30, SIDE, ui::CONTENT_TOP + 60, BODY_WIDTH, 80, line);

  ui::drawPill(frame, SIDE, AGAIN_TOP, BODY_WIDTH, "PLAY AGAIN", true, ui::ROW_HEIGHT);
  ui::drawPill(frame, SIDE, DONE_TOP, BODY_WIDTH, "DONE", true, ui::ROW_HEIGHT);
}

ResultTap resultTapAt(int x, int y) {
  if (ui::isInBox(x, y, SIDE, AGAIN_TOP, BODY_WIDTH, ui::ROW_HEIGHT)) return ResultTap::Again;
  if (ui::isInBox(x, y, SIDE, DONE_TOP, BODY_WIDTH, ui::ROW_HEIGHT)) return ResultTap::Done;
  return ResultTap::Nothing;
}

}  // namespace knuckleui
