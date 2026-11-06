#include "minesweeper_screens.h"

#include <cstdio>

#include "core/graphics.h"
#include "core/text.h"
#include "minesweeper_icons.h"
#include "minesweeper_save.h"
#include "ui/ui.h"

namespace mineui {

namespace {

namespace ms = minesweeper;

// Eight 60 px cells are exactly the 480 px screen: the minefield is full bleed, one
// continuous surface with no gaps. It starts under the warning line, below a bar.
constexpr int BOARD_WIDTH = CELL * ms::kColumns;
constexpr int BOARD_HEIGHT = CELL * ms::kRows;
constexpr int BOARD_LEFT = (SCREEN_WIDTH - BOARD_WIDTH) / 2;
constexpr int BOARD_TOP = ui::CONTENT_TOP + ui::RULE;

constexpr int SIDE = 16;
constexpr int BODY_WIDTH = SCREEN_WIDTH - 2 * SIDE;
constexpr int BOTTOM = SCREEN_HEIGHT - 16;
constexpr int PILL_TOP = BOTTOM - ui::PILL_HEIGHT;

// The strip: the count on the left, a flag beside it, and DIG/FLAG on the right.
constexpr int MODE_WIDTH = BODY_WIDTH / 3;
constexpr int MODE_LEFT = SIDE + BODY_WIDTH - MODE_WIDTH;
constexpr int MARK_SIDE = ui::PILL_HEIGHT;
constexpr int COUNT_WIDTH = BODY_WIDTH - MODE_WIDTH - MARK_SIDE - 12;

void lightGrey(uint8_t* frame, int left, int top, int width, int height) {
  for (int y = top; y < top + height; y++) {
    for (int x = left; x < left + width; x++) {
      if (x % 2 == 0 && y % 2 == 0) setPixel(frame, x, y, true);
    }
  }
}

void digitIn(uint8_t* frame, Font font, int left, int top, int size, int count, bool black = true) {
  const char digit[2] = {static_cast<char>('0' + count), '\0'};
  drawTextInBox(frame, font, left, top, size, size, digit, black);
}

// The flag, Lucide's, scaled to two thirds of the box as CrossPlay draws it.
// Nearest-neighbour, since a one-bit icon has no grey to blend.
void drawFlag(uint8_t* frame, int left, int top, int width, int height, bool white = false) {
  const int side = width < height ? width : height;
  const int inset = side / 6;
  const int size = side - inset * 2;
  const int x0 = left + (width - side) / 2 + inset;
  const int y0 = top + (height - side) / 2 + inset;
  constexpr int ROW_BYTES = minesweeperIcons::SIZE / 8;
  for (int y = 0; y < size; y++) {
    const int sy = y * minesweeperIcons::SIZE / size;
    for (int x = 0; x < size; x++) {
      const int sx = x * minesweeperIcons::SIZE / size;
      if ((minesweeperIcons::FLAG[sy * ROW_BYTES + sx / 8] & (0x80 >> (sx % 8))) == 0) setPixel(frame, x0 + x, y0 + y, !white);
    }
  }
}

// A mine: a disc of stacked bars with four spikes, CrossPlay's.
void drawMine(uint8_t* frame, int left, int top, int width, int height, bool white) {
  const int cx = left + width / 2;
  const int cy = top + height / 2;
  static const int8_t HALF_WIDTH[] = {4, 7, 9, 10, 10, 9, 7, 4};
  for (int i = 0; i < 8; ++i) fillRect(frame, cx - HALF_WIDTH[i], cy - 10 + i * 2 + 1, HALF_WIDTH[i] * 2, 2, !white);
  fillRect(frame, cx - 1, cy - 16, 3, 6, !white);
  fillRect(frame, cx - 1, cy + 11, 3, 6, !white);
  fillRect(frame, cx - 16, cy - 1, 6, 3, !white);
  fillRect(frame, cx + 11, cy - 1, 6, 3, !white);
}

// A fragment of minefield for a lesson, from a picture string: '.' covered, 'o'
// open, '*' a covered mine, 'F' a flag on a mine, 'M' a mine laid bare. Numbers are
// counted from the picture's own mines, so a diagram can't disagree with its rule.
void lessonField(uint8_t* frame, int left, int top, int cell, const char* const* rows, int columns, int rowCount) {
  const auto mineAt = [&](int c, int r) {
    if (c < 0 || c >= columns || r < 0 || r >= rowCount) return false;
    const char mark = rows[r][c];
    return mark == '*' || mark == 'F' || mark == 'M';
  };
  for (int r = 0; r < rowCount; ++r) {
    for (int c = 0; c < columns; ++c) {
      const int x = left + c * cell;
      const int y = top + r * cell;
      const char mark = rows[r][c];
      if (mark != 'o' && mark != 'M') lightGrey(frame, x, y, cell, cell);
      drawRectOutline(frame, x, y, cell, cell, 2);
      if (mark == 'F') {
        drawFlag(frame, x, y, cell, cell);
      } else if (mark == 'M') {
        fillRect(frame, x, y, cell, cell, true);
        drawMine(frame, x, y, cell, cell, true);
      } else if (mark == 'o') {
        int touching = 0;
        for (int dc = -1; dc <= 1; ++dc) {
          for (int dr = -1; dr <= 1; ++dr) {
            if ((dc != 0 || dr != 0) && mineAt(c + dc, r + dr)) ++touching;
          }
        }
        if (touching > 0) digitIn(frame, Font::Size30, x, y, cell, touching);
      }
    }
  }
}

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
    "A NUMBER COUNTS THE MINES TOUCHING IT, CORNERS INCLUDED.",
    "TAP TO DIG. HOLD TO PLANT A FLAG. THE BUTTON UNDER THE BOARD SWITCHES WHAT A TAP DOES.",
    "YOUR FIRST DIG IS ALWAYS SAFE, AND ALWAYS OPENS A SPACE.",
    "OPEN EVERY SAFE CELL AND THE FIELD IS CLEARED. FLAGS ARE NOTES: NONE ARE NEEDED TO WIN.",
    "TAP A NUMBER THAT HAS ALL ITS FLAGS AND THE REST OPENS. A WRONG FLAG ENDS IT.",
};
constexpr int LESSON_COUNT = sizeof(LESSONS) / sizeof(LESSONS[0]);
constexpr int LESSON_TEXT_HEIGHT = 150;

}  // namespace

// --- The board ---

void cellOrigin(int column, int row, int& left, int& top) {
  left = BOARD_LEFT + column * CELL;
  top = BOARD_TOP + row * CELL;
}

bool cellAt(int x, int y, int& column, int& row) {
  const int dx = x - BOARD_LEFT;
  const int dy = y - BOARD_TOP;
  if (dx < 0 || dy < 0 || dx >= BOARD_WIDTH || dy >= BOARD_HEIGHT) return false;
  column = dx / CELL;
  row = dy / CELL;
  return true;
}

bool isOnMode(int x, int y) { return ui::isInBox(x, y, MODE_LEFT, PILL_TOP, MODE_WIDTH, ui::PILL_HEIGHT); }
bool isOnVerdict(int x, int y) { return ui::isInBox(x, y, SIDE, PILL_TOP, BODY_WIDTH, ui::PILL_HEIGHT); }

void drawBoard(uint8_t* frame, const BoardModel& model) {
  const ms::Game& game = *model.game;
  ui::drawBand(frame, "MINESWEEPER", nullptr, true);

  // Bars tight against the first and last rows, so the field reads as one object.
  fillRect(frame, 0, BOARD_TOP - ui::RULE, SCREEN_WIDTH, ui::RULE, true);
  fillRect(frame, 0, BOARD_TOP + BOARD_HEIGHT, SCREEN_WIDTH, ui::RULE, true);

  const bool over = ms::over(game);
  for (int column = 0; column < ms::kColumns; ++column) {
    for (int row = 0; row < ms::kRows; ++row) {
      int x = 0;
      int y = 0;
      cellOrigin(column, row, x, y);
      const uint8_t cell = game.cell[column][row];
      const bool revealed = (cell & ms::kRevealed) != 0;

      if (!revealed) lightGrey(frame, x, y, CELL, CELL);
      drawRectOutline(frame, x, y, CELL, CELL, ui::HAIRLINE);

      if (cell & ms::kFlagged) {
        drawFlag(frame, x, y, CELL, CELL);
      } else if (revealed && (cell & ms::kMine)) {
        fillRect(frame, x, y, CELL, CELL, true);
        drawMine(frame, x, y, CELL, CELL, true);
      } else if (over && (cell & ms::kMine)) {
        drawMine(frame, x, y, CELL, CELL, false);
      } else if (revealed) {
        const int touching = ms::neighbouringMines(game, column, row);
        if (touching > 0) digitIn(frame, Font::Size30, x, y, CELL, touching);
      }
    }
  }

  // The cell under a finger, so a hold can be seen building.
  if (model.holdColumn >= 0 && model.holdRow >= 0) {
    int x = 0;
    int y = 0;
    cellOrigin(model.holdColumn, model.holdRow, x, y);
    drawRectOutline(frame, x, y, CELL, CELL, ui::FRAME);
  }

  // A settled board stays, mines bared, with its verdict where the tools were.
  if (over) {
    ui::drawPill(frame, SIDE, PILL_TOP, BODY_WIDTH, game.status == ms::Status::Won ? "CLEARED" : "BOOM", true);
    return;
  }

  // What's left to find, named with its total, and what a tap will do.
  char line[16];
  snprintf(line, sizeof(line), "%d OF %d", ms::minesRemaining(game), ms::kMines);
  drawTextInBox(frame, Font::Size30, SIDE, PILL_TOP, COUNT_WIDTH, ui::PILL_HEIGHT, line);
  drawFlag(frame, SIDE + COUNT_WIDTH, PILL_TOP, MARK_SIDE, MARK_SIDE);
  ui::drawPill(frame, MODE_LEFT, PILL_TOP, MODE_WIDTH, model.flagMode ? "FLAG" : "DIG", model.flagMode);
}

// --- The menu ---

void drawMenu(uint8_t* frame, const MenuModel& model) {
  ui::drawBand(frame, "MINESWEEPER", nullptr, true);

  char record[48];
  snprintf(record, sizeof(record), "%d PLAYED   %d CLEARED", model.wins + model.losses, model.wins);
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
  if (!model.hasHistory || model.lastBoard == nullptr) return;

  // The last settled field, at a size that carries its own numbers.
  const ms::Game& board = *model.lastBoard;
  constexpr int MINI = 34;
  constexpr int WIDTH = MINI * ms::kColumns;
  constexpr int HEIGHT = MINI * ms::kRows;
  constexpr int STACK = HEIGHT + 10 + 24;
  const int roomTop = ruleTop + ui::RULE;
  const int room = rows.topOf(0) - 12 - roomTop;
  const int top = roomTop + (room > STACK ? (room - STACK) / 2 : 0);
  const int left = (SCREEN_WIDTH - WIDTH) / 2;
  for (int column = 0; column < ms::kColumns; ++column) {
    for (int row = 0; row < ms::kRows; ++row) {
      const int x = left + column * MINI;
      const int y = top + row * MINI;
      const uint8_t cell = board.cell[column][row];
      if (cell & ms::kMine) {
        // Every mine laid bare: where they were is the memory worth keeping.
        fillRect(frame, x, y, MINI, MINI, true);
      } else if ((cell & ms::kRevealed) == 0) {
        lightGrey(frame, x, y, MINI, MINI);
        drawRectOutline(frame, x, y, MINI, MINI, 1);
        if (cell & ms::kFlagged) drawFlag(frame, x, y, MINI, MINI);
      } else {
        drawRectOutline(frame, x, y, MINI, MINI, 1);
        const int touching = ms::neighbouringMines(board, column, row);
        if (touching > 0) digitIn(frame, Font::Size10, x, y, MINI, touching);
      }
    }
  }
  drawRectOutline(frame, left - 2, top - 2, WIDTH + 4, HEIGHT + 4, 2);
  drawTextInBox(frame, Font::Size10, SIDE, top + HEIGHT + 10, BODY_WIDTH, 24, ms::boardLost(board) ? "LAST GAME: BOOM" : "LAST GAME: CLEARED");
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

  // CrossPlay's body cut on every page. The longest lessons take four lines, more
  // than the space CrossPlay gives them, so the diagram starts below the text.
  const int textHeight = drawWrapped(frame, Font::Size20, SIDE, ui::CONTENT_TOP, BODY_WIDTH, LESSONS[page], Align::Center);

  static const char* const COUNT[] = {"M..", ".oM", "..M"};
  static const char* const TOOLS[] = {"oF."};
  static const char* const FLOOD[] = {"oo...", "oo**.", "ooo..", "ooo*."};
  static const char* const WON[] = {"ooooo", "ooo*o", "ooooo", "ooFoo"};
  static const char* const CHORD[] = {".F.", ".o.", "..."};
  const char* const* rows = COUNT;
  int columns = 3;
  int rowCount = 3;
  int cell = 88;
  if (page == 1) {
    rows = TOOLS;
    rowCount = 1;
  } else if (page == 2 || page == 3) {
    rows = page == 2 ? FLOOD : WON;
    columns = 5;
    rowCount = 4;
    cell = 64;
  } else if (page == 4) {
    rows = CHORD;
  }
  const int bandTop = ui::CONTENT_TOP + (textHeight > LESSON_TEXT_HEIGHT ? textHeight : LESSON_TEXT_HEIGHT) + 10;
  const int bandBottom = PILL_TOP - 12;
  const int width = cell * columns;
  const int height = cell * rowCount;
  const int room = bandBottom - bandTop;
  lessonField(frame, (SCREEN_WIDTH - width) / 2, bandTop + (room > height ? (room - height) / 2 : 0), cell, rows, columns, rowCount);
}

bool isOnHowToNext(int x, int y) { return ui::isInBox(x, y, SIDE, PILL_TOP, BODY_WIDTH, ui::PILL_HEIGHT); }

// --- The result ---

namespace {
constexpr int AGAIN_TOP = PILL_TOP - 12 - ui::PILL_HEIGHT;
}

void drawResult(uint8_t* frame, const ResultModel& model) {
  ui::drawBand(frame, model.won ? "CLEARED" : "BOOM", nullptr, true);
  ui::drawPill(frame, SIDE, PILL_TOP, BODY_WIDTH, "DONE", true);
  ui::drawPill(frame, SIDE, AGAIN_TOP, BODY_WIDTH, "PLAY AGAIN", true);

  const int top = ui::CONTENT_TOP;
  drawTextInBox(frame, Font::Size20, SIDE, top + 30, BODY_WIDTH, 60, model.won ? "YOU CLEARED THE FIELD" : "YOU HIT A MINE");
  char line[48];
  snprintf(line, sizeof(line), "%d OF %d CELLS OPENED", model.revealed, ms::kCells - ms::kMines);
  drawTextInBox(frame, Font::Size20, SIDE, top + 110, BODY_WIDTH, 60, line);
  snprintf(line, sizeof(line), "%d OF %d MINES FLAGGED", model.flagsRight, ms::kMines);
  drawTextInBox(frame, Font::Size20, SIDE, top + 180, BODY_WIDTH, 60, line);
}

ResultTap resultTapAt(int x, int y) {
  if (ui::isInBox(x, y, SIDE, AGAIN_TOP, BODY_WIDTH, ui::PILL_HEIGHT)) return ResultTap::Again;
  if (ui::isInBox(x, y, SIDE, PILL_TOP, BODY_WIDTH, ui::PILL_HEIGHT)) return ResultTap::Done;
  return ResultTap::Nothing;
}

}  // namespace mineui
