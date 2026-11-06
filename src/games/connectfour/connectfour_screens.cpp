#include "connectfour_screens.h"

#include <cstdio>

#include "core/graphics.h"
#include "core/text.h"
#include "ui/ui.h"

namespace c4ui {

namespace {

namespace c4 = connectfour;

constexpr int SIDE = 16;
constexpr int BODY_WIDTH = SCREEN_WIDTH - 2 * SIDE;
constexpr int BOTTOM = SCREEN_HEIGHT - 16;
constexpr int PILL_TOP = BOTTOM - ui::PILL_HEIGHT;

// The board, CrossPlay's sizes: 64 px cells in a 9 px frame, with a 42 px lip
// across the top holding the disc you're about to drop. The frame starts under
// PocketPlay's warning line.
constexpr int CELL = 64;
constexpr int GRID_WIDTH = CELL * c4::kColumns;
constexpr int GRID_HEIGHT = CELL * c4::kRows;
constexpr int GRID_LEFT = (SCREEN_WIDTH - GRID_WIDTH) / 2;
constexpr int DISC_RADIUS = 26;
constexpr int RING_WEIGHT = 4;
constexpr int LIP_RADIUS = 16;
constexpr int SLOT_HEIGHT = 42;
constexpr int SLOT_TOP = ui::CONTENT_TOP + ui::BOARD_FRAME;
constexpr int GRID_TOP = SLOT_TOP + SLOT_HEIGHT;
constexpr int DISCS_PER_SIDE = c4::kCells / 2;

// The result's buttons, and the board centred in the room above them.
constexpr int AGAIN_TOP = PILL_TOP - ui::GAP - ui::PILL_HEIGHT;
constexpr int RESULT_AREA_BOTTOM = AGAIN_TOP - ui::GAP;

void lightGrey(uint8_t* frame, int left, int top, int width, int height) {
  for (int y = top; y < top + height; y++) {
    for (int x = left; x < left + width; x++) {
      if (x % 2 == 0 && y % 2 == 0) setPixel(frame, x, y, true);
    }
  }
}

void disc(uint8_t* frame, int cx, int cy, int radius, bool black) { drawRing(frame, cx, cy, radius, radius + 1, black); }

// A dimmed slot: a disc of dark-grey dither, the same size as a waiting one.
void darkGreyDisc(uint8_t* frame, int cx, int cy, int radius) {
  for (int dy = -radius; dy <= radius; dy++) {
    for (int dx = -radius; dx <= radius; dx++) {
      if (dx * dx + dy * dy > radius * radius) continue;
      const int x = cx + dx;
      const int y = cy + dy;
      setPixel(frame, x, y, (x + y) % 2 == 0);
    }
  }
}

// A disc with a black rim, filled white for light or black for dark.
void ring(uint8_t* frame, int cx, int cy, int radius, int weight, uint8_t side) {
  disc(frame, cx, cy, radius, side == c4::kDark);
  drawRing(frame, cx, cy, radius, weight, true);
}

// A stroke of squares from one point to another, for the winning line.
void line(uint8_t* frame, int x0, int y0, int x1, int y1, int weight, bool black) {
  const int dx = x1 > x0 ? x1 - x0 : x0 - x1;
  const int dy = y1 > y0 ? y1 - y0 : y0 - y1;
  const int steps = dx > dy ? dx : dy;
  for (int step = 0; step <= steps; ++step) {
    const int x = x0 + (x1 - x0) * step / (steps == 0 ? 1 : steps);
    const int y = y0 + (y1 - y0) * step / (steps == 0 ? 1 : steps);
    fillRect(frame, x - weight / 2, y - weight / 2, weight, weight, black);
  }
}

int centreX(int column) { return GRID_LEFT + column * CELL + CELL / 2; }
int centreY(int top, int row) { return top + (c4::kRows - 1 - row) * CELL + CELL / 2; }

// The board in its frame, on a dithered slab with every place punched out as a
// white hole. With a lip, the slots across the top show where a disc can go: a
// waiting disc in each open column on your turn, and a dimmed one otherwise. The
// last disc is ringed, or, given a line, the winning four are struck through.
void drawGrid(uint8_t* frame, int top, int lip, const c4::Game& game, uint8_t waiting, const int* markLine) {
  const int lipTop = top - lip;
  const int f = ui::BOARD_FRAME;
  drawRectOutline(frame, GRID_LEFT - f, lipTop - f, GRID_WIDTH + 2 * f, GRID_HEIGHT + lip + 2 * f, f);
  lightGrey(frame, GRID_LEFT, lipTop, GRID_WIDTH, GRID_HEIGHT + lip);

  if (lip > 0) {
    for (int column = 0; column < c4::kColumns; ++column) {
      const int cx = centreX(column);
      const int cy = lipTop + lip / 2;
      if ((waiting & (1 << column)) != 0) {
        ring(frame, cx, cy, LIP_RADIUS, RING_WEIGHT, c4::kLight);
      } else {
        darkGreyDisc(frame, cx, cy, LIP_RADIUS);
      }
    }
    fillRect(frame, GRID_LEFT, top - ui::RULE, GRID_WIDTH, ui::RULE, true);
  }

  for (int column = 0; column < c4::kColumns; ++column) {
    for (int row = 0; row < c4::kRows; ++row) {
      const int cx = centreX(column);
      const int cy = centreY(top, row);
      disc(frame, cx, cy, DISC_RADIUS, false);
      const uint8_t cell = game.cell[column][row];
      if (cell != c4::kEmpty) ring(frame, cx, cy, DISC_RADIUS, RING_WEIGHT, cell);
    }
  }

  if (markLine == nullptr && game.lastColumn != c4::kNoColumn) {
    const bool solid = game.cell[game.lastColumn][game.lastRow] == c4::kDark;
    disc(frame, centreX(game.lastColumn), centreY(top, game.lastRow), 8, !solid);
  }

  if (markLine != nullptr && markLine[0] >= 0 && markLine[c4::kLine - 1] >= 0) {
    const int firstColumn = markLine[0] / c4::kRows;
    const int firstRow = markLine[0] % c4::kRows;
    const int lastColumn = markLine[c4::kLine - 1] / c4::kRows;
    const int lastRow = markLine[c4::kLine - 1] % c4::kRows;
    const bool white = game.cell[firstColumn][firstRow] == c4::kDark;
    line(frame, centreX(firstColumn), centreY(top, firstRow), centreX(lastColumn), centreY(top, lastRow), 7, !white);
  }
}

// The last game at a small size, for the menu, with the winning four marked.
void miniSlab(uint8_t* frame, int left, int top, int cell, const uint8_t* cells, const int* winning) {
  const int width = cell * c4::kColumns;
  const int height = cell * c4::kRows;
  drawRectOutline(frame, left - ui::FRAME, top - ui::FRAME, width + ui::FRAME * 2, height + ui::FRAME * 2, ui::FRAME);
  lightGrey(frame, left, top, width, height);
  const int radius = cell * 26 / 64;
  for (int column = 0; column < c4::kColumns; ++column) {
    for (int row = 0; row < c4::kRows; ++row) {
      const int cx = left + column * cell + cell / 2;
      const int cy = top + (c4::kRows - 1 - row) * cell + cell / 2;
      disc(frame, cx, cy, radius, false);
      const uint8_t piece = cells[column * c4::kRows + row];
      if (piece != c4::kEmpty) ring(frame, cx, cy, radius, 3, piece);
    }
  }
  if (winning == nullptr || winning[0] < 0) return;
  const int weight = cell / 10 < 2 ? 2 : cell / 10;
  for (int i = 0; i < c4::kLine; ++i) {
    const int column = winning[i] / c4::kRows;
    const int row = winning[i] % c4::kRows;
    const int cx = left + column * cell + cell / 2;
    const int cy = top + (c4::kRows - 1 - row) * cell + cell / 2;
    fillRoundRect(frame, cx - weight * 2, cy - weight * 2, weight * 4, weight * 4, weight, true);
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
    "TAP A COLUMN. YOUR DISC FALLS TO THE LOWEST FREE PLACE IN IT.",
    "FOUR IN A LINE WINS. ACROSS, UP, OR ON EITHER DIAGONAL LIKE THIS ONE.",
    "A FULL COLUMN STOPS TAKING DISCS, AND ITS SLOT AT THE TOP GOES GREY.",
};
constexpr int LESSON_COUNT = sizeof(LESSONS) / sizeof(LESSONS[0]);

}  // namespace

// --- The board ---

int columnAt(int x, int y) {
  if (y < SLOT_TOP || y >= GRID_TOP + GRID_HEIGHT + ui::BOARD_FRAME) return c4::kNoColumn;
  if (x < GRID_LEFT || x >= GRID_LEFT + GRID_WIDTH) return c4::kNoColumn;
  return (x - GRID_LEFT) / CELL;
}

void drawBoard(uint8_t* frame, const BoardModel& model) {
  const c4::Game& game = *model.game;
  ui::drawBand(frame, "CONNECT FOUR", nullptr, true);

  // The status capsule: whose drop it is.
  fillRect(frame, SIDE, PILL_TOP, BODY_WIDTH, ui::PILL_HEIGHT, true);
  drawTextInBox(frame, Font::Size20, SIDE, PILL_TOP, BODY_WIDTH, ui::PILL_HEIGHT, model.yourTurn ? "YOUR DROP" : "THEIR DROP", false);

  uint8_t open = 0;
  if (model.yourTurn) {
    for (int column = 0; column < c4::kColumns; ++column) {
      if (c4::canDrop(game, column)) open |= static_cast<uint8_t>(1 << column);
    }
  }
  drawGrid(frame, GRID_TOP, SLOT_HEIGHT, game, open, nullptr);

  // The discs each side has left, theirs above yours, as a count and a row.
  int played[2] = {0, 0};
  for (int column = 0; column < c4::kColumns; ++column) {
    for (int row = 0; row < c4::kRows; ++row) {
      if (game.cell[column][row] == c4::kLight) ++played[0];
      if (game.cell[column][row] == c4::kDark) ++played[1];
    }
  }
  constexpr int PITCH = 18;
  constexpr int SMALL = 8;
  constexpr int ROW_PITCH = SMALL * 2 + ui::GAP;
  constexpr int BAND_TOP = PILL_TOP - ui::GAP * 2 - ROW_PITCH * 2;
  for (int strip = 0; strip < 2; ++strip) {
    const uint8_t side = strip == 0 ? c4::kDark : c4::kLight;
    const int held = DISCS_PER_SIDE - played[side == c4::kLight ? 0 : 1];
    const int cy = BAND_TOP + strip * ROW_PITCH + SMALL;
    char count[8];
    snprintf(count, sizeof(count), "%d", held);
    ui::drawTextLeft(frame, Font::Size10, GRID_LEFT, cy - 11, 22, count);
    for (int i = 0; i < held; ++i) ring(frame, GRID_LEFT + 44 + SMALL + i * PITCH, cy, SMALL, 2, side);
  }
}

// --- The menu ---

void drawMenu(uint8_t* frame, const MenuModel& model) {
  ui::drawBand(frame, "CONNECT FOUR", nullptr, true);

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
  if (!model.hasHistory || model.lastCells == nullptr) return;

  // The last game's final position, with its four marked.
  constexpr int MINI = 30;
  constexpr int WIDTH = MINI * c4::kColumns;
  constexpr int HEIGHT = MINI * c4::kRows;
  constexpr int BLOCK = HEIGHT + 14 + 24;
  const int roomTop = ruleTop + ui::RULE;
  const int room = rows.topOf(0) - ui::GAP - roomTop;
  const int top = roomTop + (room > BLOCK ? (room - BLOCK) / 2 : 12);
  miniSlab(frame, (SCREEN_WIDTH - WIDTH) / 2, top, MINI, model.lastCells, model.lastLine);
  const char* caption = "LAST GAME: A DRAW";
  if (model.lastOutcome == 0) caption = "LAST GAME: YOU WON";
  if (model.lastOutcome == 1) caption = "LAST GAME: THEY WON";
  drawTextInBox(frame, Font::Size10, SIDE, top + HEIGHT + 14, BODY_WIDTH, 24, caption);
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
  drawWrapped(frame, Font::Size20, SIDE, ui::CONTENT_TOP, BODY_WIDTH, LESSONS[page], Align::Center);

  // A four by four corner of a board, in its frame, centred in the room left.
  constexpr int SIDE_CELLS = 4;
  constexpr int FRAGMENT = CELL * SIDE_CELLS;
  constexpr int BAND_TOP = ui::CONTENT_TOP + 150 + 46;
  constexpr int AREA_BOTTOM = PILL_TOP - ui::GAP;
  const int top = BAND_TOP + (AREA_BOTTOM - BAND_TOP - FRAGMENT) / 2;
  const int left = (SCREEN_WIDTH - FRAGMENT) / 2;
  const int f = ui::BOARD_FRAME;
  drawRectOutline(frame, left - f, top - f, FRAGMENT + 2 * f, FRAGMENT + 2 * f, f);
  lightGrey(frame, left, top, FRAGMENT, FRAGMENT);

  static const char* const BOARDS[3][SIDE_CELLS] = {
      {"....", "....", ".L..", ".D.."},
      {"...L", "..LD", ".LDD", "LDLD"},
      {"D...", "L...", "D...", "LD.."},
  };
  for (int row = 0; row < SIDE_CELLS; ++row) {
    for (int column = 0; column < SIDE_CELLS; ++column) {
      const int cx = left + column * CELL + CELL / 2;
      const int cy = top + row * CELL + CELL / 2;
      disc(frame, cx, cy, DISC_RADIUS, false);
      const char c = BOARDS[page][row][column];
      if (c != '.') ring(frame, cx, cy, DISC_RADIUS, RING_WEIGHT, c == 'L' ? c4::kLight : c4::kDark);
    }
  }
  // The disc about to drop, above the column it falls down.
  if (page == 0) ring(frame, left + CELL + CELL / 2, top - f - SLOT_HEIGHT / 2, LIP_RADIUS, RING_WEIGHT, c4::kLight);
  // The diagonal four, struck through.
  if (page == 1) line(frame, left + CELL / 2, top + 3 * CELL + CELL / 2, left + 3 * CELL + CELL / 2, top + CELL / 2, 7, true);
}

bool isOnHowToNext(int x, int y) { return ui::isInBox(x, y, SIDE, PILL_TOP, BODY_WIDTH, ui::PILL_HEIGHT); }

// --- The result ---

void drawResult(uint8_t* frame, const ResultModel& model) {
  const c4::Game& game = *model.game;
  const char* headline = "A DRAW";
  if (game.outcome == c4::Outcome::LightWins) headline = "YOU WIN";
  if (game.outcome == c4::Outcome::DarkWins) headline = "THEY WIN";
  ui::drawBand(frame, headline, nullptr, true);

  ui::drawPill(frame, SIDE, AGAIN_TOP, BODY_WIDTH, "PLAY AGAIN", true);
  ui::drawPill(frame, SIDE, PILL_TOP, BODY_WIDTH, "DONE", true);

  // No slots here, so the board sits centred in the room above the buttons.
  const int slack = RESULT_AREA_BOTTOM - ui::CONTENT_TOP - GRID_HEIGHT - ui::BOARD_FRAME * 2;
  drawGrid(frame, ui::CONTENT_TOP + (slack > 0 ? slack / 2 : 0) + ui::BOARD_FRAME, 0, game, 0, model.line);
}

ResultTap resultTapAt(int x, int y) {
  if (ui::isInBox(x, y, SIDE, AGAIN_TOP, BODY_WIDTH, ui::PILL_HEIGHT)) return ResultTap::Again;
  if (ui::isInBox(x, y, SIDE, PILL_TOP, BODY_WIDTH, ui::PILL_HEIGHT)) return ResultTap::Done;
  return ResultTap::Nothing;
}

}  // namespace c4ui
