#include "wordle_screens.h"

#include <cstdio>

#include "core/graphics.h"
#include "core/text.h"
#include "ui/ui.h"
#include "wordle_icons.h"

namespace wordleui {

namespace {

using wordle::Mark;

constexpr Font DISPLAY_FONT = Font::Size30;
constexpr Font BODY_FONT = Font::Size20;
constexpr Font SMALL_FONT = Font::Size10;

constexpr int MARGIN = 16;
constexpr int INNER_WIDTH = SCREEN_WIDTH - 2 * MARGIN;
constexpr int BODY_TOP = ui::CONTENT_TOP;
constexpr int HAIRLINE = 1;
constexpr int RULE = 3;
constexpr int FRAME = 4;

// The game.
constexpr int MESSAGE_HEIGHT = 22;
constexpr int TILE = 60;
constexpr int TILE_GAP = 8;
constexpr int GRID_TOP = BODY_TOP + MESSAGE_HEIGHT + 4;
constexpr int GRID_WIDTH = wordle::kLetters * TILE + (wordle::kLetters - 1) * TILE_GAP;
constexpr int GRID_LEFT = (SCREEN_WIDTH - GRID_WIDTH) / 2;
constexpr int GRID_BOTTOM = GRID_TOP + wordle::kRows * TILE + (wordle::kRows - 1) * TILE_GAP;

constexpr int KEYS_TOP = GRID_BOTTOM + 16;
constexpr int KEY_GAP = 6;
constexpr int KEY_HEIGHT = 74;
constexpr int KEY_WIDTH = (INNER_WIDTH - 9 * KEY_GAP) / 10;
constexpr int WIDE_KEY_WIDTH = KEY_WIDTH * 3 / 2 + KEY_GAP / 2;
constexpr const char* KEY_ROWS[3] = {"QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"};
constexpr int KEY_ROW_LENGTHS[3] = {10, 9, 7};

constexpr int FOOTER_HEIGHT = ui::PILL_HEIGHT;
constexpr int FOOTER_TOP = SCREEN_HEIGHT - MARGIN - FOOTER_HEIGHT;
constexpr int FOOTER_BUTTON_WIDTH = (INNER_WIDTH - 12) / 2;
constexpr int FOOTER_NEXT_LEFT = SCREEN_WIDTH - MARGIN - FOOTER_BUTTON_WIDTH;

// The menu.
constexpr int MENU_ROW_HEIGHT = 62;
constexpr int MENU_ROW_STEP = MENU_ROW_HEIGHT + 4;
constexpr int MENU_ROWS_TOP = SCREEN_HEIGHT - MARGIN - 3 * MENU_ROW_STEP + 4;
constexpr int MENU_TODAY_HEIGHT = 96;
constexpr int CHART_TOP = BODY_TOP + 166;
constexpr int CHART_ROW_HEIGHT = 30;
constexpr int CHART_ROW_STEP = CHART_ROW_HEIGHT + 8;
constexpr int CHART_LABEL_WIDTH = 28;
constexpr int CHART_COUNT_WIDTH = 44;
constexpr int CHART_BAR_MAX = INNER_WIDTH - CHART_LABEL_WIDTH - CHART_COUNT_WIDTH - 16;

// The calendar.
constexpr int CALENDAR_MARGIN = ui::MARGIN;
constexpr int CALENDAR_WIDTH = SCREEN_WIDTH - 2 * CALENDAR_MARGIN;
constexpr int STEPPER_TOP = BODY_TOP + 12;
constexpr int STEPPER_HEIGHT = 62;
constexpr int STEPPER_STEP = STEPPER_HEIGHT + 4;
constexpr int STEPPER_BUTTON_WIDTH = 52;
constexpr int STEPPER_VALUE_WIDTH = 196;  // fits SEPTEMBER
constexpr int STEPPER_PLUS_LEFT = CALENDAR_MARGIN + CALENDAR_WIDTH - 12 - STEPPER_BUTTON_WIDTH;
constexpr int STEPPER_MINUS_LEFT = STEPPER_PLUS_LEFT - 12 - STEPPER_VALUE_WIDTH - STEPPER_BUTTON_WIDTH;
constexpr int PILL_HEIGHT = ui::PILL_HEIGHT;
constexpr int PILL_TOP = SCREEN_HEIGHT - MARGIN - PILL_HEIGHT;
constexpr int CELL_COLUMNS = 7;
constexpr int CELL_ROWS = 6;
constexpr int CELL_GAP = 3;
constexpr int CELL = (CALENDAR_WIDTH - (CELL_COLUMNS - 1) * CELL_GAP) / CELL_COLUMNS;
constexpr int WEEKDAY_HEIGHT = 28;
constexpr int CALENDAR_HEIGHT = WEEKDAY_HEIGHT + CELL_ROWS * CELL + (CELL_ROWS - 1) * CELL_GAP;
constexpr int CALENDAR_AREA_TOP = STEPPER_TOP + 2 * STEPPER_STEP + 12;
constexpr int CALENDAR_TOP = CALENDAR_AREA_TOP + (PILL_TOP - CALENDAR_AREA_TOP - CALENDAR_HEIGHT) / 2;
constexpr int CELLS_TOP = CALENDAR_TOP + WEEKDAY_HEIGHT;

// One dot in four, the light grey CrossPlay uses for a letter that isn't in the word.
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

void drawIcon(uint8_t* frame, const uint8_t* bits, int left, int top, bool black) {
  constexpr int ROW_BYTES = ICON_SIZE / 8;
  for (int y = 0; y < ICON_SIZE; y++) {
    for (int x = 0; x < ICON_SIZE; x++) {
      const uint8_t byte = bits[y * ROW_BYTES + x / 8];
      if ((byte & (0x80 >> (x % 8))) == 0) setPixel(frame, left + x, top + y, black);
    }
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
    fillRect(frame, left + inset + step - RULE / 2, top + inset + step - RULE / 2, RULE, RULE, true);
    fillRect(frame, left + inset + span - step - RULE / 2, top + inset + step - RULE / 2, RULE, RULE, true);
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

void drawLeftText(uint8_t* frame, Font font, int left, int top, int height, const char* text, bool black = true) {
  drawText(frame, font, left, top + (height + capHeight(font)) / 2, text, Align::Left, black);
}

// CrossPlay's black band, with BACK, the title, and a small label such as the date on
// the right. A long title drops a size rather than run into the label.
void drawChrome(uint8_t* frame, const char* title, const char* rightLabel) { ui::drawBand(frame, title, rightLabel, true, Font::Size14); }

// One tile or key. Right place: solid black. In the word: a heavy border and a dot. Not
// in the word: grey. Empty: a thin border. Typed: a firmer one.
void drawCell(uint8_t* frame, int left, int top, int width, int height, char letter, Mark mark, Font font) {
  const char text[2] = {letter, '\0'};
  bool black = true;
  switch (mark) {
    case Mark::Empty:
      drawRectOutline(frame, left, top, width, height, 1);
      break;
    case Mark::Typed:
      drawRectOutline(frame, left, top, width, height, 2);
      break;
    case Mark::Absent:
      fillGrey(frame, left, top, width, height);
      break;
    case Mark::Present: {
      drawRectOutline(frame, left, top, width, height, 3);
      const int radius = width >= 48 ? 5 : 3;
      drawRing(frame, left + width - radius - 4, top + radius + 4, radius, radius, true);
      break;
    }
    case Mark::Correct:
      fillRect(frame, left, top, width, height, true);
      black = false;
      break;
  }
  if (letter != '\0') drawTextInBox(frame, font, left, top, width, height, text, black);
}

int keyLeft(int row, int index) {
  const int count = KEY_ROW_LENGTHS[row];
  const int extra = row == 2 ? 2 * (WIDE_KEY_WIDTH + KEY_GAP) : 0;
  const int width = count * KEY_WIDTH + (count - 1) * KEY_GAP + extra;
  const int rowLeft = (SCREEN_WIDTH - width) / 2;

  // Row 3 opens with ENTER, so its letters start one wide key in.
  return rowLeft + (row == 2 ? WIDE_KEY_WIDTH + KEY_GAP : 0) + index * (KEY_WIDTH + KEY_GAP);
}

int keyTop(int row) { return KEYS_TOP + row * (KEY_HEIGHT + KEY_GAP); }

int enterLeft() { return keyLeft(2, 0) - KEY_GAP - WIDE_KEY_WIDTH; }

int eraseLeft() { return keyLeft(2, KEY_ROW_LENGTHS[2] - 1) + KEY_WIDTH + KEY_GAP; }

void drawIconKey(uint8_t* frame, int left, const uint8_t* icon, bool filled, bool enabled) {
  const int top = keyTop(2);
  if (!enabled) {
    fillGrey(frame, left, top, WIDE_KEY_WIDTH, KEY_HEIGHT);
    drawRectOutline(frame, left, top, WIDE_KEY_WIDTH, KEY_HEIGHT, 1);
  } else if (filled) {
    fillRect(frame, left, top, WIDE_KEY_WIDTH, KEY_HEIGHT, true);
  } else {
    drawRectOutline(frame, left, top, WIDE_KEY_WIDTH, KEY_HEIGHT, 2);
  }
  drawIcon(frame, icon, left + (WIDE_KEY_WIDTH - ICON_SIZE) / 2, top + (KEY_HEIGHT - ICON_SIZE) / 2, !(filled && enabled));
}

void drawKeyboard(uint8_t* frame, const wordle::Game& game) {
  for (int row = 0; row < 3; row++) {
    for (int i = 0; i < KEY_ROW_LENGTHS[row]; i++) {
      const char letter = KEY_ROWS[row][i];
      drawCell(frame, keyLeft(row, i), keyTop(row), KEY_WIDTH, KEY_HEIGHT, letter, game.key(letter), BODY_FONT);
    }
  }
  drawIconKey(frame, enterLeft(), ICON_TICK, true, game.typed() == wordle::kLetters);
  drawIconKey(frame, eraseLeft(), ICON_DELETE, false, true);
}

void drawFooterButton(uint8_t* frame, int left, const char* label, bool enabled) {
  if (enabled) {
    ui::drawPill(frame, left, FOOTER_TOP, FOOTER_BUTTON_WIDTH, label, true, FOOTER_HEIGHT);
  } else {
    ui::drawDimmedPill(frame, left, FOOTER_TOP, FOOTER_BUTTON_WIDTH, label, FOOTER_HEIGHT);
  }
}

void drawFinished(uint8_t* frame, const wordle::Game& game, bool canNext) {
  char verdict[24];
  if (game.status() == wordle::Game::Status::Won) {
    snprintf(verdict, sizeof(verdict), "SOLVED IN %d", game.guesses());
  } else {
    snprintf(verdict, sizeof(verdict), "NOT SOLVED");
  }
  const int areaTop = GRID_BOTTOM + 16;
  drawTextInBox(frame, DISPLAY_FONT, 0, areaTop, SCREEN_WIDTH, FOOTER_TOP - areaTop, verdict);

  drawFooterButton(frame, MARGIN, "ARCHIVE", true);
  drawFooterButton(frame, FOOTER_NEXT_LEFT, "NEXT", canNext);
}

void drawMenuRow(uint8_t* frame, int index, const char* label, const char* value, bool enabled) {
  const int top = MENU_ROWS_TOP + index * MENU_ROW_STEP;
  // A hairline row, like the home page's. A spent one is greyed, not removed.
  if (!enabled) fillGrey(frame, MARGIN, top, INNER_WIDTH, MENU_ROW_HEIGHT);
  drawRectOutline(frame, MARGIN, top, INNER_WIDTH, MENU_ROW_HEIGHT, HAIRLINE);
  drawLeftText(frame, BODY_FONT, MARGIN + 12, top, MENU_ROW_HEIGHT, label);
  if (value[0] != '\0') {
    drawText(frame, BODY_FONT, SCREEN_WIDTH - MARGIN - 12, top + (MENU_ROW_HEIGHT + capHeight(BODY_FONT)) / 2, value, Align::Right);
  }
}

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
  drawRectOutline(frame, CALENDAR_MARGIN, top, CALENDAR_WIDTH, STEPPER_HEIGHT, HAIRLINE);
  drawLeftText(frame, BODY_FONT, CALENDAR_MARGIN + 12, top, STEPPER_HEIGHT, label);
  drawTextInBox(frame, BODY_FONT, STEPPER_MINUS_LEFT + STEPPER_BUTTON_WIDTH + 6, top, STEPPER_VALUE_WIDTH, STEPPER_HEIGHT, value);
  drawStepperButton(frame, STEPPER_MINUS_LEFT, top, "-", canDown);
  drawStepperButton(frame, STEPPER_PLUS_LEFT, top, "+", canUp);
}

// A sparkle for a solve, a cross for a loss, and a ring for a day opened and left, so a
// month reads at a glance before any number does.
void drawDayMark(uint8_t* frame, int left, int top, int size, const CalendarDay& day) {
  if (day.lost) {
    drawCross(frame, left, top, size);
  } else if (day.finished) {
    drawSparkle(frame, left, top, size);
  } else {
    drawRing(frame, left + size / 2, top + size / 2, size / 2 - 3, HAIRLINE, true);
  }
}

void drawCalendarCell(uint8_t* frame, int left, int top, const CalendarDay& day, bool isToday) {
  if (day.inArchive) drawRectOutline(frame, left, top, CELL, CELL, HAIRLINE);
  if (isToday) drawRectOutline(frame, left, top, CELL, CELL, FRAME);

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

void drawGame(uint8_t* frame, const GameModel& model) {
  const wordle::Game& game = *model.game;

  if (game.over()) {
    char answer[wordle::kLetters + 1] = {};
    for (int i = 0; i < wordle::kLetters; i++) {
      answer[i] = game.answer()[i];
    }
    char result[12];
    if (game.status() == wordle::Game::Status::Won) {
      snprintf(result, sizeof(result), "%d / %d", game.guesses(), wordle::kRows);
    } else {
      snprintf(result, sizeof(result), "X / %d", wordle::kRows);
    }
    drawChrome(frame, answer, result);
  } else {
    drawChrome(frame, "WORDLE", model.date);
  }

  if (model.message != nullptr) drawTextInBox(frame, SMALL_FONT, 0, BODY_TOP, SCREEN_WIDTH, MESSAGE_HEIGHT, model.message);

  for (int row = 0; row < wordle::kRows; row++) {
    for (int column = 0; column < wordle::kLetters; column++) {
      char letter = '\0';
      Mark mark = Mark::Empty;
      if (row < game.guesses()) {
        letter = game.guess(row)[column];
        mark = game.marks(row)[column];
      } else if (row == game.guesses() && !game.over() && column < game.typed()) {
        letter = game.typing()[column];
        mark = Mark::Typed;
      }
      drawCell(frame, GRID_LEFT + column * (TILE + TILE_GAP), GRID_TOP + row * (TILE + TILE_GAP), TILE, TILE, letter, mark, DISPLAY_FONT);
    }
  }

  if (game.over()) {
    drawFinished(frame, game, model.canNext);
  } else {
    drawKeyboard(frame, game);
  }
}

char keyAt(int x, int y) {
  const int lastRowTop = keyTop(2);
  if (ui::isInBox(x, y, enterLeft(), lastRowTop, WIDE_KEY_WIDTH, KEY_HEIGHT)) return KEY_ENTER;
  if (ui::isInBox(x, y, eraseLeft(), lastRowTop, WIDE_KEY_WIDTH, KEY_HEIGHT)) return KEY_ERASE;
  for (int row = 0; row < 3; row++) {
    for (int i = 0; i < KEY_ROW_LENGTHS[row]; i++) {
      if (ui::isInBox(x, y, keyLeft(row, i), keyTop(row), KEY_WIDTH, KEY_HEIGHT)) return KEY_ROWS[row][i];
    }
  }
  return 0;
}

bool isOnFinishedArchive(int x, int y) { return ui::isInBox(x, y, MARGIN, FOOTER_TOP, FOOTER_BUTTON_WIDTH, FOOTER_HEIGHT); }

bool isOnFinishedNext(int x, int y) { return ui::isInBox(x, y, FOOTER_NEXT_LEFT, FOOTER_TOP, FOOTER_BUTTON_WIDTH, FOOTER_HEIGHT); }

void drawMenu(uint8_t* frame, const MenuModel& model) {
  drawChrome(frame, "WORDLE", nullptr);

  // Today, in the biggest type, is also the way into today's game.
  drawLeftText(frame, DISPLAY_FONT, MARGIN, BODY_TOP, 60, model.date != nullptr ? model.date : "NONE YET");
  drawLeftText(frame, BODY_FONT, MARGIN, BODY_TOP + 60, 30, model.state);
  fillRect(frame, MARGIN, BODY_TOP + 104, INNER_WIDTH, RULE, true);

  char text[64];
  snprintf(text, sizeof(text), "%d PLAYED   %d WON   STREAK %d", model.stats.played, model.stats.won, model.stats.streak);
  drawLeftText(frame, SMALL_FONT, MARGIN, BODY_TOP + 118, 24, text);

  // The one piece of decoration is the player's own record: how many guesses each win
  // took, as bars.
  int most = 1;
  for (const int wins : model.stats.wins) {
    if (wins > most) most = wins;
  }
  const int trackLeft = MARGIN + CHART_LABEL_WIDTH + 8;
  for (int i = 0; i < wordle::kRows; i++) {
    const int top = CHART_TOP + i * CHART_ROW_STEP;
    const int wins = model.stats.wins[i];

    snprintf(text, sizeof(text), "%d", i + 1);
    drawTextInBox(frame, BODY_FONT, MARGIN, top, CHART_LABEL_WIDTH, CHART_ROW_HEIGHT, text);

    drawRectOutline(frame, trackLeft, top, CHART_BAR_MAX, CHART_ROW_HEIGHT, HAIRLINE);
    if (wins > 0) fillRect(frame, trackLeft, top, (CHART_BAR_MAX * wins + most - 1) / most, CHART_ROW_HEIGHT, true);

    snprintf(text, sizeof(text), "%d", wins);
    drawTextInBox(frame, BODY_FONT, trackLeft + CHART_BAR_MAX + 8, top, CHART_COUNT_WIDTH, CHART_ROW_HEIGHT, text);
  }
  drawTextInBox(frame, SMALL_FONT, 0, CHART_TOP + wordle::kRows * CHART_ROW_STEP + 4, SCREEN_WIDTH, 24, "GUESSES PER WIN");

  snprintf(text, sizeof(text), "%d", model.puzzles);
  drawMenuRow(frame, 0, "HOW TO PLAY", "", true);
  drawMenuRow(frame, 1, "ARCHIVE", text, true);
  drawMenuRow(frame, 2, model.upToDate ? "ALL CAUGHT UP" : "GET PUZZLES", model.upToDate ? "" : "WI-FI", !model.upToDate);
}

MenuTap menuTapAt(int x, int y) {
  if (ui::isInBox(x, y, MARGIN, BODY_TOP, INNER_WIDTH, MENU_TODAY_HEIGHT)) return MenuTap::Today;
  const MenuTap rows[3] = {MenuTap::HowTo, MenuTap::Archive, MenuTap::GetPuzzles};
  for (int i = 0; i < 3; i++) {
    if (ui::isInBox(x, y, MARGIN, MENU_ROWS_TOP + i * MENU_ROW_STEP, INNER_WIDTH, MENU_ROW_HEIGHT)) return rows[i];
  }
  return MenuTap::Nothing;
}

void drawHowTo(uint8_t* frame) {
  drawChrome(frame, "HOW TO PLAY", nullptr);

  int y = BODY_TOP + 12;
  drawLeftText(frame, BODY_FONT, MARGIN, y, lineHeight(BODY_FONT), "SIX GUESSES FOR ONE WORD");
  y += lineHeight(BODY_FONT) + 6;
  y += drawWrapped(frame, SMALL_FONT, MARGIN, y, INNER_WIDTH, "Type a five-letter word and tap the tick. Each letter is then marked.") + 24;

  struct Example {
    char letter;
    Mark mark;
    const char* meaning;
  };
  constexpr Example EXAMPLES[] = {
      {'P', Mark::Correct, "In the word, in this spot"},
      {'L', Mark::Present, "In the word, somewhere else"},
      {'C', Mark::Absent, "Not in the word"},
  };
  for (const Example& example : EXAMPLES) {
    drawCell(frame, MARGIN, y, TILE, TILE, example.letter, example.mark, DISPLAY_FONT);
    drawLeftText(frame, SMALL_FONT, MARGIN + TILE + 16, y, TILE, example.meaning);
    y += TILE + 16;
  }
  drawWrapped(frame, SMALL_FONT, MARGIN, y + 8, INNER_WIDTH, "A new word every day. Past days are in the archive.");
}

void drawCalendar(uint8_t* frame, const CalendarModel& model) {
  constexpr const char* MONTH_NAMES[12] = {"JANUARY", "FEBRUARY", "MARCH", "APRIL", "MAY", "JUNE", "JULY", "AUGUST", "SEPTEMBER", "OCTOBER", "NOVEMBER", "DECEMBER"};
  constexpr const char* WEEKDAYS[CELL_COLUMNS] = {"S", "M", "T", "W", "T", "F", "S"};

  char text[16];
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

  ui::drawPill(frame, MARGIN, PILL_TOP, INNER_WIDTH, "TODAY", true, PILL_HEIGHT);
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
    ui::drawDimmedPill(frame, MARGIN, PILL_TOP, INNER_WIDTH, "WORKING", PILL_HEIGHT);
  } else {
    ui::drawPill(frame, MARGIN, PILL_TOP, INNER_WIDTH, model.done ? "PLAY" : "BACK", true, PILL_HEIGHT);
  }

  // An outline while it's working, and solid black once finished: the inversion says
  // "done", and it costs nothing because a finished panel never repaints.
  constexpr int PANEL_HEIGHT = 190;
  const int panelTop = BODY_TOP + (PILL_TOP - BODY_TOP - PANEL_HEIGHT) / 2;
  const bool ink = !model.done;
  if (model.done) {
    fillRect(frame, MARGIN, panelTop, INNER_WIDTH, PANEL_HEIGHT, true);
  } else {
    drawRectOutline(frame, MARGIN, panelTop, INNER_WIDTH, PANEL_HEIGHT, FRAME);
  }

  if (working) {
    constexpr int TEXT_INSET = 16;
    drawWrapped(frame, BODY_FONT, MARGIN + TEXT_INSET, panelTop + 4, INNER_WIDTH - 2 * TEXT_INSET, "THIS TAKES A MINUTE. BACK STOPS IT.", Align::Center);
  } else {
    char number[16];
    snprintf(number, sizeof(number), "%d", model.puzzles);
    drawTextInBox(frame, DISPLAY_FONT, MARGIN, panelTop + 28, INNER_WIDTH, 52, model.failed ? "--" : number, ink);
  }

  fillRect(frame, MARGIN + 70, panelTop + 96, INNER_WIDTH - 140, RULE, ink);
  fillRect(frame, MARGIN + 70, panelTop + 104, INNER_WIDTH - 140, HAIRLINE, ink);

  drawTextInBox(frame, BODY_FONT, MARGIN + 12, panelTop + 124, INNER_WIDTH - 24, 56, model.done ? "PUZZLES ON THE CARD" : model.detail, ink);
}

}  // namespace wordleui
