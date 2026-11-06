#include "connections_screens.h"

#include <cstdio>
#include <cstring>

#include "connections_fit.h"
#include "core/graphics.h"
#include "core/text.h"
#include "ui/ui.h"

namespace connectionsui {

namespace fit = connectionsfit;

namespace {

static_assert(fit::BOARD_WIDTH == SCREEN_WIDTH, "the tile sizes in connections_fit.h assume the screen's width");

constexpr Font BODY_FONT = Font::Size20;
constexpr Font SMALL_FONT = Font::Size10;
constexpr Font LABEL_FONT = Font::Size14;  // the band's right label: smaller than the title, as in Wordle

constexpr int MARGIN = 16;
constexpr int INNER_WIDTH = SCREEN_WIDTH - 2 * MARGIN;
constexpr int BODY_TOP = ui::CONTENT_TOP;
constexpr int GUTTER = 12;
constexpr int PILL_HEIGHT = ui::PILL_HEIGHT;
constexpr int PILL_TOP = SCREEN_HEIGHT - MARGIN - PILL_HEIGHT;

// --- Board geometry. The actions sit on the bottom margin, the status above them, and
// the four rows of tiles are centred in what's left. A solved group takes over the row
// its tiles held, so nothing below the block ever moves. ---
constexpr int GRID_LEFT = fit::GRID_MARGIN;
constexpr int GRID_WIDTH = fit::GRID_WIDTH;
constexpr int ACTIONS_TOP = PILL_TOP;
constexpr int STATUS_HEIGHT = 34;
constexpr int STATUS_TOP = ACTIONS_TOP - GUTTER - STATUS_HEIGHT;
constexpr int BOARD_AREA_BOTTOM = STATUS_TOP - GUTTER;
constexpr int BLOCK_HEIGHT = connections::kGroups * fit::ROW_HEIGHT + (connections::kGroups - 1) * fit::TILE_GAP;
constexpr int BLOCK_TOP = BODY_TOP + (BOARD_AREA_BOTTOM - BODY_TOP - BLOCK_HEIGHT) / 2;
static_assert(BLOCK_TOP >= BODY_TOP, "the board must fit between the header and the status");
constexpr int ACTION_WIDTH = (GRID_WIDTH - 2 * GUTTER) / 3;
constexpr int PIP = 16;
constexpr int PIP_STEP = PIP + 8;

// --- Menu geometry, the same front door as Wordle's. ---
constexpr int MENU_TODAY_HEIGHT = 96;
constexpr int MENU_ROW_HEIGHT = ui::ROW_HEIGHT;
constexpr int MENU_ROW_STEP = MENU_ROW_HEIGHT + ui::ROW_GAP;
constexpr int MENU_ROWS_TOP = SCREEN_HEIGHT - MARGIN - 3 * MENU_ROW_STEP + ui::ROW_GAP;
constexpr int RECENT_SIZE = 196;
constexpr int RECENT_LEFT = MARGIN + (INNER_WIDTH - RECENT_SIZE) / 2;
constexpr int RECENT_TOP = BODY_TOP + 200;

// --- Calendar geometry. ---
constexpr int CALENDAR_MARGIN = fit::GRID_MARGIN;
constexpr int CALENDAR_WIDTH = SCREEN_WIDTH - 2 * CALENDAR_MARGIN;
constexpr int STEPPER_TOP = BODY_TOP + 12;
constexpr int STEPPER_HEIGHT = ui::ROW_HEIGHT;
constexpr int STEPPER_STEP = STEPPER_HEIGHT + ui::ROW_GAP;
constexpr int STEPPER_BUTTON_WIDTH = 52;
constexpr int STEPPER_VALUE_WIDTH = 196;  // fits SEPTEMBER
constexpr int STEPPER_PLUS_LEFT = CALENDAR_MARGIN + CALENDAR_WIDTH - 12 - STEPPER_BUTTON_WIDTH;
constexpr int STEPPER_MINUS_LEFT = STEPPER_PLUS_LEFT - 12 - STEPPER_VALUE_WIDTH - STEPPER_BUTTON_WIDTH;
constexpr int CELL_COLUMNS = 7;
constexpr int CELL_ROWS = 6;
constexpr int CELL_GAP = 3;
constexpr int CELL = (CALENDAR_WIDTH - (CELL_COLUMNS - 1) * CELL_GAP) / CELL_COLUMNS;
constexpr int WEEKDAY_HEIGHT = 28;
constexpr int CALENDAR_HEIGHT = WEEKDAY_HEIGHT + CELL_ROWS * CELL + (CELL_ROWS - 1) * CELL_GAP;
constexpr int CALENDAR_AREA_TOP = STEPPER_TOP + 2 * STEPPER_STEP + 12;
constexpr int CALENDAR_TOP = CALENDAR_AREA_TOP + (PILL_TOP - CALENDAR_AREA_TOP - CALENDAR_HEIGHT) / 2;
constexpr int CELLS_TOP = CALENDAR_TOP + WEEKDAY_HEIGHT;

void drawChrome(uint8_t* frame, const char* title, const char* rightLabel) { ui::drawBand(frame, title, rightLabel, true, LABEL_FONT); }

// One dot in four, the only grey there is.
void fillGrey(uint8_t* frame, int left, int top, int width, int height) {
  for (int y = top; y < top + height; y++) {
    for (int x = left; x < left + width; x++) {
      if (x % 2 == 0 && y % 2 == 0) setPixel(frame, x, y, true);
    }
  }
}

// Text can't be drawn in grey, so this takes every other dot back out of it.
void fadeToGrey(uint8_t* frame, int left, int top, int width, int height) {
  for (int y = top; y < top + height; y++) {
    for (int x = left; x < left + width; x++) {
      if ((x + y) % 2 == 0) setPixel(frame, x, y, false);
    }
  }
}

void drawDot(uint8_t* frame, int left, int top, int size, bool filled, bool black = true) {
  if (filled) {
    fillRoundRect(frame, left, top, size, size, size / 2, black);
  } else {
    drawRoundRectOutline(frame, left, top, size, size, size / 2, ui::HAIRLINE, black);
  }
}

// Each row between the triangle's edges, found by walking them with whole numbers.
void fillTriangle(uint8_t* frame, int x0, int y0, int x1, int y1, int x2, int y2) {
  const int points[3][2] = {{x0, y0}, {x1, y1}, {x2, y2}};
  int top = y0;
  int bottom = y0;
  for (const auto& point : points) {
    if (point[1] < top) top = point[1];
    if (point[1] > bottom) bottom = point[1];
  }
  for (int y = top; y <= bottom; y++) {
    int low = SCREEN_WIDTH;
    int high = -1;
    for (int i = 0; i < 3; i++) {
      const int* a = points[i];
      const int* b = points[(i + 1) % 3];
      if ((y < a[1] && y < b[1]) || (y > a[1] && y > b[1])) continue;
      if (a[1] == b[1]) {
        if (a[0] < low) low = a[0];
        if (b[0] < low) low = b[0];
        if (a[0] > high) high = a[0];
        if (b[0] > high) high = b[0];
        continue;
      }
      const int x = a[0] + (b[0] - a[0]) * (y - a[1]) / (b[1] - a[1]);
      if (x < low) low = x;
      if (x > high) high = x;
    }
    if (high >= low) fillRect(frame, low, y, high - low + 1, 1, true);
  }
}

// A square brush stepped along each diagonal, like Tic Tac Toe's X.
void drawCross(uint8_t* frame, int left, int top, int size) {
  const int inset = size / 5;
  const int span = size - 2 * inset;
  for (int step = 0; step <= span; step++) {
    fillRect(frame, left + inset + step - ui::RULE / 2, top + inset + step - ui::RULE / 2, ui::RULE, ui::RULE, true);
    fillRect(frame, left + inset + span - step - ui::RULE / 2, top + inset + step - ui::RULE / 2, ui::RULE, ui::RULE, true);
  }
}

// Four tapered arms, each a pair of triangles meeting at the centre.
void drawSparkle(uint8_t* frame, int left, int top, int size) {
  const int x = left + size / 2;
  const int y = top + size / 2;
  const int reach = size / 2;
  const int waist = reach / 3;
  fillTriangle(frame, x, y - reach, x - waist, y, x + waist, y);
  fillTriangle(frame, x, y + reach, x - waist, y, x + waist, y);
  fillTriangle(frame, x - reach, y, x, y - waist, x, y + waist);
  fillTriangle(frame, x + reach, y, x, y - waist, x, y + waist);
}

// Corner brackets, the shape D&Diagrams' map and CrossPlay's chess board wear.
void drawBrackets(uint8_t* frame, int left, int top, int size, int arm) {
  constexpr int W = ui::FRAME;
  for (int cx = 0; cx < 2; cx++) {
    for (int cy = 0; cy < 2; cy++) {
      const int x = cx == 0 ? left : left + size - arm;
      const int y = cy == 0 ? top : top + size - W;
      fillRect(frame, x, y, arm, W, true);
      const int vx = cx == 0 ? left : left + size - W;
      const int vy = cy == 0 ? top : top + size - arm;
      fillRect(frame, vx, vy, W, arm, true);
    }
  }
}

// Already-measured lines, each centred in the box's width on a line of its own.
void drawLines(uint8_t* frame, Font font, int left, int top, int width, const char* text, const fit::Lines& lines, bool black) {
  const int height = lineHeight(font);
  for (int i = 0; i < lines.count; i++) {
    const int span = lines.end[i] - lines.begin[i];
    if (span <= 0 || span >= fit::MAX_LINE_CHARS) continue;
    char part[fit::MAX_LINE_CHARS] = {};
    memcpy(part, text + lines.begin[i], static_cast<size_t>(span));
    drawTextInBox(frame, font, left, top + i * height, width, height, part, black);
  }
}

// --- The board ---

int rowTop(int row) { return BLOCK_TOP + row * (fit::ROW_HEIGHT + fit::TILE_GAP); }

int tileLeft(int column) { return GRID_LEFT + column * (fit::TILE_WIDTH + fit::TILE_GAP); }

void drawSolvedRow(uint8_t* frame, int top, const connections::Group& group) {
  // Solid black: a solved row never repaints again, so its ink costs nothing.
  fillRect(frame, GRID_LEFT, top, GRID_WIDTH, fit::ROW_HEIGHT, true);

  fit::SolvedRow row;
  fit::layoutSolvedRow(group, fit::SOLVED_INNER_WIDTH, fit::ROW_HEIGHT, row);
  int gap = fit::ROW_HEIGHT - row.nameHeight - row.listHeight;
  if (gap > fit::TILE_GAP) gap = fit::TILE_GAP;
  if (gap < 0) gap = 0;
  const int left = GRID_LEFT + fit::TILE_PAD;
  const int textTop = top + (fit::ROW_HEIGHT - row.nameHeight - gap - row.listHeight) / 2;
  drawLines(frame, row.nameFont, left, textTop, fit::SOLVED_INNER_WIDTH, group.name, row.name, false);
  drawLines(frame, row.listFont, left, textTop + row.nameHeight + gap, fit::SOLVED_INNER_WIDTH, row.words, row.list, false);
}

void drawTile(uint8_t* frame, int left, int top, const char* word, bool chosen, const fit::TileCut& cut) {
  if (chosen) {
    fillRect(frame, left, top, fit::TILE_WIDTH, fit::ROW_HEIGHT, true);
  } else {
    drawRectOutline(frame, left, top, fit::TILE_WIDTH, fit::ROW_HEIGHT, ui::HAIRLINE);
  }
  fit::Lines lines;
  fit::layoutTile(word, fit::TILE_INNER_WIDTH, cut, lines);
  const int innerTop = top + 2;
  const int textTop = innerTop + (fit::TILE_INNER_HEIGHT - lines.count * lineHeight(cut.font)) / 2;
  drawLines(frame, cut.font, left + fit::TILE_PAD, textTop, fit::TILE_INNER_WIDTH, word, lines, !chosen);
}

void drawStatus(uint8_t* frame, const BoardModel& model) {
  const connections::Game& game = *model.game;
  // Mistakes as four pips that empty out: countable at a glance, where a number needs
  // reading.
  const char* label = model.toast != nullptr ? model.toast : "MISTAKES";
  const int pipBand = connections::kMaxMistakes * PIP_STEP;
  const int labelWidth = GRID_WIDTH - pipBand - GUTTER;
  const Font font = textWidth(BODY_FONT, label) <= labelWidth ? BODY_FONT : Font::Size14;
  ui::drawTextLeft(frame, font, GRID_LEFT, STATUS_TOP, STATUS_HEIGHT, label);
  for (int i = 0; i < connections::kMaxMistakes; i++) {
    const int left = GRID_LEFT + GRID_WIDTH - (connections::kMaxMistakes - i) * PIP_STEP;
    drawDot(frame, left, STATUS_TOP + (STATUS_HEIGHT - PIP) / 2, PIP, i < game.mistakesLeft());
  }
}

void drawActions(uint8_t* frame, const connections::Game& game) {
  if (game.result() != connections::Result::Playing) {
    ui::drawPill(frame, GRID_LEFT, ACTIONS_TOP, GRID_WIDTH, game.result() == connections::Result::Won ? "SOLVED - BACK TO LIST" : "BACK TO LIST", true);
    return;
  }
  const char* labels[3] = {"SHUFFLE", "CLEAR", "SUBMIT"};
  for (int i = 0; i < 3; i++) {
    const int left = GRID_LEFT + i * (ACTION_WIDTH + GUTTER);
    // SUBMIT is off only when there's nothing to submit. A repeat stays pressable, so
    // the player is told ALREADY GUESSED rather than left wondering.
    if (i == 2 && !game.canSubmit()) {
      ui::drawDimmedPill(frame, left, ACTIONS_TOP, ACTION_WIDTH, labels[i]);
    } else {
      ui::drawPill(frame, left, ACTIONS_TOP, ACTION_WIDTH, labels[i], true);
    }
  }
}

// --- The menu ---

void drawRecentGrid(uint8_t* frame, const MenuModel& model) {
  constexpr int STEP = RECENT_SIZE / 4;
  constexpr int SIZE = STEP - 6;
  for (int i = 0; i < 16; i++) {
    const int left = RECENT_LEFT + (i % 4) * STEP;
    const int top = RECENT_TOP + (i / 4) * STEP;
    // The last cell is the newest puzzle, and wears the heavier border.
    const int border = i == 15 ? ui::RULE : ui::HAIRLINE;
    switch (model.recent[i]) {
      case DayResult::Perfect:
        fillRect(frame, left, top, SIZE, SIZE, true);
        break;
      case DayResult::Won:
        fillGrey(frame, left, top, SIZE, SIZE);
        drawRectOutline(frame, left, top, SIZE, SIZE, border);
        break;
      case DayResult::Lost:
        drawRectOutline(frame, left, top, SIZE, SIZE, border);
        drawCross(frame, left + 10, top + 10, SIZE - 20);
        break;
      case DayResult::Unplayed:
        drawRectOutline(frame, left, top, SIZE, SIZE, border);
        break;
    }
  }
}

void drawMenuRow(uint8_t* frame, int index, const char* label, const char* value, bool enabled) {
  const int top = MENU_ROWS_TOP + index * MENU_ROW_STEP;
  // A hairline row, like the home page's. A spent one is greyed, not removed.
  if (!enabled) fillGrey(frame, MARGIN, top, INNER_WIDTH, MENU_ROW_HEIGHT);
  drawRectOutline(frame, MARGIN, top, INNER_WIDTH, MENU_ROW_HEIGHT, ui::HAIRLINE);
  ui::drawTextLeft(frame, BODY_FONT, MARGIN + 14, top, MENU_ROW_HEIGHT, label);
  if (value[0] != '\0') {
    drawText(frame, BODY_FONT, SCREEN_WIDTH - MARGIN - 14, top + (MENU_ROW_HEIGHT + capHeight(BODY_FONT)) / 2, value, Align::Right);
  }
}

const char* todayState(const MenuModel& model) {
  if (model.todayDone) return "SOLVED";
  if (model.inProgress) return "IN PROGRESS";
  return "NOT STARTED";
}

// --- The calendar ---

// Spent, not missing: grey, so it still reads as a control and shows which end of the
// archive has been reached.
void drawStepperButton(uint8_t* frame, int left, int top, const char* label, bool live) {
  const int buttonTop = top + 6;
  const int height = STEPPER_HEIGHT - 12;
  if (!live) fillGrey(frame, left, buttonTop, STEPPER_BUTTON_WIDTH, height);
  drawRectOutline(frame, left, buttonTop, STEPPER_BUTTON_WIDTH, height, live ? 2 : 1);
  drawTextInBox(frame, BODY_FONT, left, buttonTop, STEPPER_BUTTON_WIDTH, height, label);
}

void drawStepper(uint8_t* frame, int top, const char* label, const char* value, bool canDown, bool canUp) {
  drawRectOutline(frame, CALENDAR_MARGIN, top, CALENDAR_WIDTH, STEPPER_HEIGHT, ui::HAIRLINE);
  ui::drawTextLeft(frame, BODY_FONT, CALENDAR_MARGIN + 12, top, STEPPER_HEIGHT, label);
  drawTextInBox(frame, BODY_FONT, STEPPER_MINUS_LEFT + STEPPER_BUTTON_WIDTH + 6, top, STEPPER_VALUE_WIDTH, STEPPER_HEIGHT, value);
  drawStepperButton(frame, STEPPER_MINUS_LEFT, top, "-", canDown);
  drawStepperButton(frame, STEPPER_PLUS_LEFT, top, "+", canUp);
}

// A sparkle for a clean solve, pips for the mistakes, and a cross for a loss, so a
// month reads at a glance before any number does. Pips rather than a digit: a digit
// under the date read as part of it.
void drawDayMark(uint8_t* frame, int left, int top, int size, const CalendarDay& day) {
  if (day.lost) {
    drawCross(frame, left, top, size);
    return;
  }
  if (day.mistakes == 0) {
    drawSparkle(frame, left, top, size);
    return;
  }
  const int pip = size / 3;  // CrossPlay uses a quarter, which is 6 px in this cell
  const int span = day.mistakes * pip + (day.mistakes - 1) * 3;
  const int start = left + (size - span) / 2;
  for (int i = 0; i < day.mistakes; i++) drawDot(frame, start + i * (pip + 3), top + (size - pip) / 2, pip, true);
}

void drawCalendarCell(uint8_t* frame, int left, int top, const CalendarDay& day, bool isToday) {
  if (day.inArchive) drawRectOutline(frame, left, top, CELL, CELL, ui::HAIRLINE);
  // Today gets a heavier frame, so the day that matters most needn't be read to be found.
  if (isToday) drawRectOutline(frame, left, top, CELL, CELL, ui::FRAME);

  constexpr int NUMBER_HEIGHT = 22;
  const int numberTop = top + 6;
  char number[4];
  snprintf(number, sizeof(number), "%d", day.day);
  drawTextInBox(frame, SMALL_FONT, left, numberTop, CELL, NUMBER_HEIGHT, number);

  // A date with no puzzle is greyed rather than hidden, so the month keeps its shape.
  if (!day.inArchive) fadeToGrey(frame, left, numberTop, CELL, NUMBER_HEIGHT);

  if (day.played) {
    const int markTop = numberTop + NUMBER_HEIGHT + 2;
    const int room = top + CELL - 5 - markTop;
    const int size = room < CELL / 2 ? room : CELL / 2;
    drawDayMark(frame, left + (CELL - size) / 2, markTop + (room - size) / 2, size, day);
  }
}

}  // namespace

void formatDate(uint32_t date, char* out, int size) {
  static const char* MONTHS[12] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
  const int year = static_cast<int>(date / 10000);
  const int month = static_cast<int>(date / 100 % 100);
  const int day = static_cast<int>(date % 100);
  if (month < 1 || month > 12) {
    snprintf(out, static_cast<size_t>(size), "%u", static_cast<unsigned>(date));
    return;
  }
  snprintf(out, static_cast<size_t>(size), "%d %s %d", day, MONTHS[month - 1], year);
}

void drawBoard(uint8_t* frame, const BoardModel& model) {
  // The date is the title: the game's name is on the row tapped to get here, and what
  // identifies this screen is which puzzle it is.
  char date[16];
  formatDate(model.date, date, sizeof(date));
  drawChrome(frame, date, nullptr);
  if (model.game == nullptr) return;
  const connections::Game& game = *model.game;

  const int revealed = game.revealedCount();
  for (int i = 0; i < revealed; i++) drawSolvedRow(frame, rowTop(i), game.solvedGroup(i));

  // A loss reveals every group as a row, so no tiles are left to draw.
  if (game.result() != connections::Result::Lost && game.tileCount() > 0) {
    // One cut for every tile, chosen against the widest word still on the board.
    const char* words[connections::kTiles];
    for (int i = 0; i < game.tileCount(); i++) words[i] = game.tileWord(i);
    const fit::TileCut cut = fit::chooseTileCut(words, game.tileCount(), fit::TILE_INNER_WIDTH, fit::TILE_INNER_HEIGHT);
    for (int i = 0; i < game.tileCount(); i++) {
      drawTile(frame, tileLeft(i % 4), rowTop(revealed + i / 4), game.tileWord(i), game.isSelected(i), cut);
    }
  }

  drawStatus(frame, model);
  drawActions(frame, game);
}

BoardTap boardTapAt(const connections::Game& game, int x, int y, int& tile) {
  if (game.result() != connections::Result::Playing) {
    return ui::isInBox(x, y, GRID_LEFT, ACTIONS_TOP, GRID_WIDTH, PILL_HEIGHT) ? BoardTap::Done : BoardTap::Nothing;
  }
  const BoardTap actions[3] = {BoardTap::Shuffle, BoardTap::Clear, BoardTap::Submit};
  for (int i = 0; i < 3; i++) {
    if (ui::isInBox(x, y, GRID_LEFT + i * (ACTION_WIDTH + GUTTER), ACTIONS_TOP, ACTION_WIDTH, PILL_HEIGHT)) return actions[i];
  }
  const int revealed = game.revealedCount();
  for (int i = 0; i < game.tileCount(); i++) {
    if (ui::isInBox(x, y, tileLeft(i % 4), rowTop(revealed + i / 4), fit::TILE_WIDTH, fit::ROW_HEIGHT)) {
      tile = i;
      return BoardTap::Tile;
    }
  }
  return BoardTap::Nothing;
}

void drawMenu(uint8_t* frame, const MenuModel& model) {
  drawChrome(frame, "CONNECTIONS", nullptr);

  // Today's date, in the biggest type, is also the way into today's puzzle.
  char date[16] = "NONE YET";
  if (model.hasPuzzles) formatDate(model.newestDate, date, sizeof(date));
  ui::drawTextLeft(frame, Font::Size30, MARGIN, BODY_TOP, 60, date);
  ui::drawTextLeft(frame, BODY_FONT, MARGIN, BODY_TOP + 60, 30, todayState(model));
  fillRect(frame, MARGIN, BODY_TOP + 104, INNER_WIDTH, ui::RULE, true);

  char stats[64];
  snprintf(stats, sizeof(stats), "%d PLAYED   %d PERFECT   STREAK %d", model.played, model.perfect, model.streak);
  ui::drawTextLeft(frame, SMALL_FONT, MARGIN, BODY_TOP + 118, 24, stats);

  // The one piece of decoration is the shape of a Connections board, drawn from the
  // player's own last sixteen days. Tapping it opens the archive.
  drawBrackets(frame, RECENT_LEFT - 22, RECENT_TOP - 22, RECENT_SIZE + 38, 34);
  drawRecentGrid(frame, model);
  drawTextInBox(frame, SMALL_FONT, 0, RECENT_TOP + RECENT_SIZE + 26, SCREEN_WIDTH, 24, "LAST 16 DAYS");

  char total[16];
  snprintf(total, sizeof(total), "%d", model.puzzleCount);
  drawMenuRow(frame, 0, "HOW TO PLAY", "", true);
  drawMenuRow(frame, 1, "ARCHIVE", total, true);
  drawMenuRow(frame, 2, model.upToDate ? "ALL CAUGHT UP" : "GET PUZZLES", model.upToDate ? "" : "WI-FI", !model.upToDate);
}

MenuTap menuTapAt(int x, int y) {
  if (ui::isInBox(x, y, MARGIN, BODY_TOP, INNER_WIDTH, MENU_TODAY_HEIGHT)) return MenuTap::Today;
  if (ui::isInBox(x, y, RECENT_LEFT, RECENT_TOP, RECENT_SIZE, RECENT_SIZE)) return MenuTap::Archive;
  const MenuTap rows[3] = {MenuTap::HowTo, MenuTap::Archive, MenuTap::GetPuzzles};
  for (int i = 0; i < 3; i++) {
    if (ui::isInBox(x, y, MARGIN, MENU_ROWS_TOP + i * MENU_ROW_STEP, INNER_WIDTH, MENU_ROW_HEIGHT)) return rows[i];
  }
  return MenuTap::Nothing;
}

// Mostly a board: a picture of a real one, with one group already taken, says what the
// game is faster than a paragraph. The prose is only what the picture can't say.
void drawHowTo(uint8_t* frame) {
  drawChrome(frame, "HOW TO PLAY", nullptr);

  constexpr const char* HEADING = "SIXTEEN WORDS, FOUR GROUPS";
  int y = BODY_TOP + 12;
  const Font heading = textWidth(BODY_FONT, HEADING) <= INNER_WIDTH ? BODY_FONT : Font::Size14;
  ui::drawTextLeft(frame, heading, MARGIN, y, lineHeight(heading), HEADING);
  y += lineHeight(heading) + 6;
  y += drawWrapped(frame, SMALL_FONT, MARGIN, y, INNER_WIDTH, "Tap four that share something, then SUBMIT. Four wrong guesses and it is over.") + 20;

  static const char* BOARD[4][4] = {
      {"HAIL", "SLEET", "RAIN", "SNOW"},
      {"BOB", "OTTO", "ANNA", "EWE"},
      {"REED", "PIPE", "DRUM", "HORN"},
      {"MARS", "VENUS", "PLUTO", "CERES"},
  };
  constexpr int TILE_HEIGHT = 54;
  const int small = lineHeight(SMALL_FONT);
  const int solvedHeight = small * 2 + 20;
  constexpr int DEMO_WIDTH = (INNER_WIDTH - 3 * fit::TILE_GAP) / 4;

  // The taken group: one filled row, name over members, both in white.
  fillRect(frame, MARGIN, y, INNER_WIDTH, solvedHeight, true);
  drawTextInBox(frame, SMALL_FONT, MARGIN, y + 6, INNER_WIDTH, small, "WET WEATHER", false);
  char members[32];
  snprintf(members, sizeof(members), "%s, %s, %s, %s", BOARD[0][0], BOARD[0][1], BOARD[0][2], BOARD[0][3]);
  drawTextInBox(frame, SMALL_FONT, MARGIN, y + small + 12, INNER_WIDTH, small, members, false);
  y += solvedHeight + fit::TILE_GAP;
  for (int r = 1; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      const int left = MARGIN + c * (DEMO_WIDTH + fit::TILE_GAP);
      drawRectOutline(frame, left, y, DEMO_WIDTH, TILE_HEIGHT, ui::HAIRLINE);
      drawTextInBox(frame, SMALL_FONT, left, y, DEMO_WIDTH, TILE_HEIGHT, BOARD[r][c]);
    }
    y += TILE_HEIGHT + fit::TILE_GAP;
  }
  y += 12;

  y += drawWrapped(frame, SMALL_FONT, MARGIN, y, INNER_WIDTH, "WET WEATHER is solved and has closed up. ONE AWAY means three of your four were right.") + 14;

  // The pips are drawn, not described: four filled dots is what a fresh puzzle's
  // mistakes look like at the foot of the board.
  for (int i = 0; i < connections::kMaxMistakes; i++) drawDot(frame, MARGIN + i * PIP_STEP, y, PIP, true);
  const int pipBand = connections::kMaxMistakes * PIP_STEP + 8;
  ui::drawTextLeft(frame, SMALL_FONT, MARGIN + pipBand, y, PIP, "wrong guesses left");
  y += PIP + 16;

  drawWrapped(frame, SMALL_FONT, MARGIN, y, INNER_WIDTH, "SHUFFLE only moves the tiles. It never changes the answer.");
}

void drawCalendar(uint8_t* frame, const CalendarModel& model) {
  constexpr const char* MONTH_NAMES[12] = {"JANUARY", "FEBRUARY", "MARCH", "APRIL", "MAY", "JUNE", "JULY", "AUGUST", "SEPTEMBER", "OCTOBER", "NOVEMBER", "DECEMBER"};
  constexpr const char* WEEKDAYS[CELL_COLUMNS] = {"S", "M", "T", "W", "T", "F", "S"};

  char text[24];
  snprintf(text, sizeof(text), "%d PLAYED", model.playedThisMonth);
  drawChrome(frame, "ARCHIVE", text);

  snprintf(text, sizeof(text), "%d", model.year);
  drawStepper(frame, STEPPER_TOP, "YEAR", text, model.canPrevYear, model.canNextYear);
  const int month = model.month >= 1 && model.month <= 12 ? model.month : 1;
  drawStepper(frame, STEPPER_TOP + STEPPER_STEP, "MONTH", MONTH_NAMES[month - 1], model.canPrevMonth, model.canNextMonth);

  for (int column = 0; column < CELL_COLUMNS; column++) {
    drawTextInBox(frame, SMALL_FONT, CALENDAR_MARGIN + column * (CELL + CELL_GAP), CALENDAR_TOP, CELL, 22, WEEKDAYS[column]);
  }
  for (int i = 0; i < CALENDAR_CELLS; i++) {
    const CalendarDay& day = model.cells[i];
    if (day.day == 0) continue;
    const int left = CALENDAR_MARGIN + (i % CELL_COLUMNS) * (CELL + CELL_GAP);
    const int top = CELLS_TOP + (i / CELL_COLUMNS) * (CELL + CELL_GAP);
    drawCalendarCell(frame, left, top, day, i == model.todayCell);
  }

  ui::drawPill(frame, MARGIN, PILL_TOP, INNER_WIDTH, "TODAY", true);
}

CalendarTap calendarTapAt(int x, int y, int& cell) {
  if (ui::isInBox(x, y, MARGIN, PILL_TOP, INNER_WIDTH, PILL_HEIGHT)) return CalendarTap::Today;

  for (int row = 0; row < 2; row++) {
    const int top = STEPPER_TOP + row * STEPPER_STEP + 6;
    const int height = STEPPER_HEIGHT - 12;
    if (ui::isInBox(x, y, STEPPER_MINUS_LEFT, top, STEPPER_BUTTON_WIDTH, height)) return row == 0 ? CalendarTap::PrevYear : CalendarTap::PrevMonth;
    if (ui::isInBox(x, y, STEPPER_PLUS_LEFT, top, STEPPER_BUTTON_WIDTH, height)) return row == 0 ? CalendarTap::NextYear : CalendarTap::NextMonth;
  }

  const int step = CELL + CELL_GAP;
  const int dx = x - CALENDAR_MARGIN;
  const int dy = y - CELLS_TOP;
  if (dx < 0 || dy < 0) return CalendarTap::Nothing;
  const int column = dx / step;
  const int row = dy / step;
  if (column >= CELL_COLUMNS || row >= CELL_ROWS) return CalendarTap::Nothing;
  if (dx % step >= CELL || dy % step >= CELL) return CalendarTap::Nothing;
  cell = row * CELL_COLUMNS + column;
  return CalendarTap::Day;
}

void drawImport(uint8_t* frame, const ImportModel& model) {
  drawChrome(frame, "GET PUZZLES", nullptr);

  const bool working = !model.done && !model.failed;
  if (working) {
    ui::drawDimmedPill(frame, MARGIN, PILL_TOP, INNER_WIDTH, "WORKING");
  } else {
    ui::drawPill(frame, MARGIN, PILL_TOP, INNER_WIDTH, model.done ? "PLAY" : "BACK", true);
  }

  // An outline while it works, and solid black once finished: the inversion says
  // "done", and costs nothing because a finished panel never repaints.
  constexpr int PANEL_HEIGHT = 190;
  const int panelTop = BODY_TOP + (PILL_TOP - BODY_TOP - PANEL_HEIGHT) / 2;
  const bool ink = !model.done;
  if (model.done) {
    fillRect(frame, MARGIN, panelTop, INNER_WIDTH, PANEL_HEIGHT, true);
  } else {
    drawRectOutline(frame, MARGIN, panelTop, INNER_WIDTH, PANEL_HEIGHT, ui::FRAME);
  }

  if (working) {
    constexpr int TEXT_INSET = 16;
    drawWrapped(frame, BODY_FONT, MARGIN + TEXT_INSET, panelTop + 4, INNER_WIDTH - 2 * TEXT_INSET, "THIS TAKES A MINUTE. BACK STOPS IT.", Align::Center);
  } else {
    char number[16];
    snprintf(number, sizeof(number), "%d", model.puzzles);
    drawTextInBox(frame, Font::Size30, MARGIN, panelTop + 28, INNER_WIDTH, 52, model.failed ? "--" : number, ink);
  }

  // The doubled line, the header band's own motif.
  fillRect(frame, MARGIN + 70, panelTop + 96, INNER_WIDTH - 140, ui::RULE, ink);
  fillRect(frame, MARGIN + 70, panelTop + 104, INNER_WIDTH - 140, ui::HAIRLINE, ink);

  drawTextInBox(frame, BODY_FONT, MARGIN + 12, panelTop + 124, INNER_WIDTH - 24, 56, model.done ? "PUZZLES ON THE CARD" : model.detail, ink);
}

bool isOnImportButton(int x, int y) { return ui::isInBox(x, y, MARGIN, PILL_TOP, INNER_WIDTH, PILL_HEIGHT); }

}  // namespace connectionsui
