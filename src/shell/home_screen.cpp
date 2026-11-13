#include "home_screen.h"

#include <cstdio>
#include <cstring>

#include "core/battery.h"
#include "core/graphics.h"
#include "core/storage.h"
#include "core/text.h"
#include "games/games.h"
#include "home_pages.h"
#include "ui/ui.h"

namespace homeScreen {

namespace {

// The page is saved as the name of its first game, never as a number: a page
// number would point at other games once the list or rows per page changed.
constexpr const char* PAGE_FILE = "home";
constexpr uint8_t PAGE_VERSION = 1;
constexpr size_t MAX_NAME_BYTES = 32;

constexpr int ROW_WIDTH = SCREEN_WIDTH - 2 * ui::MARGIN;
constexpr int BOTTOM_MARGIN = 20;

// 4 px shorter than other lists' rows, so ten fit above the bar.
constexpr int ROW_HEIGHT = 58;

// One bar along the bottom: the battery on the left, the page marks in the middle,
// and the settings cog on the right.
constexpr int BAR_HEIGHT = 40;
constexpr int BAR_TOP = SCREEN_HEIGHT - BOTTOM_MARGIN - BAR_HEIGHT;

// The page marks: they carry only numbers.
constexpr int MARKS_HEIGHT = BAR_HEIGHT;
constexpr int MARKS_TOP = BAR_TOP;
constexpr int MARK_WIDTH = 34;
constexpr int MARK_GAP = 4;
constexpr int MARK_RADIUS = 8;
constexpr int MARK_BORDER = 2;

// The battery at its widest ("100%" and the bolt) ends at x=148. The marks are
// centred, so they may spread this far either side of the middle before narrowing:
// 4 pages (40 games) at full width.
constexpr int BATTERY_RIGHT = 148;
constexpr int MARKS_MAX_WIDTH = 2 * (SCREEN_WIDTH / 2 - BATTERY_RIGHT - ui::GAP);

constexpr int COG_SIZE = 32;
constexpr int COG_LEFT = SCREEN_WIDTH - ui::MARGIN - COG_SIZE;
constexpr int COG_TOP = BAR_TOP + (BAR_HEIGHT - COG_SIZE) / 2;
// The cog's target is larger than the cog, out to the screen's corner, so it is
// easy to hit. It stays clear of the widest marks and of the last row.
constexpr int COG_TARGET_LEFT = SCREEN_WIDTH - 80;
constexpr int COG_TARGET_TOP = BAR_TOP - ui::GAP / 2;

constexpr uint8_t COG[] = {
    0xFF, 0xFF, 0xFF, 0xFF,  // ................................
    0xFF, 0xF8, 0x1F, 0xFF,  // .............######.............
    0xFF, 0xF8, 0x1F, 0xFF,  // .............######.............
    0xFE, 0xF8, 0x1F, 0x7F,  // .......#.....######.....#.......
    0xFC, 0x78, 0x1E, 0x3F,  // ......###....######....###......
    0xF8, 0x38, 0x1C, 0x1F,  // .....#####...######...#####.....
    0xF0, 0x10, 0x08, 0x0F,  // ....#######.########.#######....
    0xE0, 0x00, 0x00, 0x07,  // ...##########################...
    0xF0, 0x00, 0x00, 0x0F,  // ....########################....
    0xF8, 0x00, 0x00, 0x1F,  // .....######################.....
    0xFC, 0x00, 0x00, 0x3F,  // ......####################......
    0xFE, 0x01, 0x80, 0x7F,  // .......########..########.......
    0xFC, 0x07, 0xE0, 0x3F,  // ......#######......#######......
    0x80, 0x0F, 0xF0, 0x01,  // .###########........###########.
    0x80, 0x0F, 0xF0, 0x01,  // .###########........###########.
    0x80, 0x1F, 0xF8, 0x01,  // .##########..........##########.
    0x80, 0x1F, 0xF8, 0x01,  // .##########..........##########.
    0x80, 0x0F, 0xF0, 0x01,  // .###########........###########.
    0x80, 0x0F, 0xF0, 0x01,  // .###########........###########.
    0xFC, 0x07, 0xE0, 0x3F,  // ......#######......#######......
    0xFE, 0x01, 0x80, 0x7F,  // .......########..########.......
    0xFC, 0x00, 0x00, 0x3F,  // ......####################......
    0xF8, 0x00, 0x00, 0x1F,  // .....######################.....
    0xF0, 0x00, 0x00, 0x0F,  // ....########################....
    0xE0, 0x00, 0x00, 0x07,  // ...##########################...
    0xF0, 0x10, 0x08, 0x0F,  // ....#######.########.#######....
    0xF8, 0x38, 0x1C, 0x1F,  // .....#####...######...#####.....
    0xFC, 0x78, 0x1E, 0x3F,  // ......###....######....###......
    0xFE, 0xF8, 0x1F, 0x7F,  // .......#.....######.....#.......
    0xFF, 0xF8, 0x1F, 0xFF,  // .............######.............
    0xFF, 0xF8, 0x1F, 0xFF,  // .............######.............
    0xFF, 0xFF, 0xFF, 0xFF,  // ................................
};

static_assert(ui::CONTENT_TOP + homePages::ROWS_PER_PAGE * (ROW_HEIGHT + ui::ROW_GAP) - ui::ROW_GAP + ui::GAP <= BAR_TOP,
              "the rows must end above the bar");
static_assert(SCREEN_WIDTH / 2 + MARKS_MAX_WIDTH / 2 + ui::GAP <= COG_TARGET_LEFT, "the marks must clear the cog's target");

int page = 0;
bool pageSaveFailed = false;

int rowTop(int row) { return ui::CONTENT_TOP + row * (ROW_HEIGHT + ui::ROW_GAP); }

int pages() { return homePages::pageCount(games::count()); }

// Marks narrow only if there are too many pages for them all at full width.
int markWidth() {
  const int count = pages();
  const int fits = (MARKS_MAX_WIDTH - (count - 1) * MARK_GAP) / count;
  return fits < MARK_WIDTH ? fits : MARK_WIDTH;
}

int marksLeft() {
  const int count = pages();
  return (SCREEN_WIDTH - (count * markWidth() + (count - 1) * MARK_GAP)) / 2;
}

void savePage() {
  if (!storage::ready() || games::count() == 0) return;
  const char* name = games::at(homePages::firstOnPage(page)).name();
  pageSaveFailed = !storage::save(PAGE_FILE, PAGE_VERSION, reinterpret_cast<const uint8_t*>(name), strlen(name));
}

bool showPage(int next) {
  if (next == page) return false;
  page = next;
  savePage();
  return true;
}

int indexOf(const char* name) {
  for (int i = 0; i < games::count(); i++) {
    if (strcmp(games::at(i).name(), name) == 0) return i;
  }
  return -1;
}

void drawMarks(uint8_t* frame) {
  const int width = markWidth();
  const Font font = width >= 32 ? Font::Size20 : Font::Size14;
  for (int i = 0; i < pages(); i++) {
    const int left = marksLeft() + i * (width + MARK_GAP);
    char number[12];
    snprintf(number, sizeof(number), "%d", i + 1);
    const bool current = i == page;
    if (current) {
      fillRoundRect(frame, left, MARKS_TOP, width, MARKS_HEIGHT, MARK_RADIUS, true);
    } else {
      drawRoundRectOutline(frame, left, MARKS_TOP, width, MARKS_HEIGHT, MARK_RADIUS, MARK_BORDER);
    }
    drawTextInBox(frame, font, left, MARKS_TOP, width, MARKS_HEIGHT, number, !current);
  }
}

void drawSettings(uint8_t* frame) { drawIcon(frame, COG, COG_SIZE, COG_SIZE, COG_LEFT, COG_TOP); }

}  // namespace

void begin() {
  page = 0;
  if (!storage::ready()) return;

  char name[MAX_NAME_BYTES + 1];
  uint8_t version = 0;
  size_t size = 0;
  if (!storage::load(PAGE_FILE, version, reinterpret_cast<uint8_t*>(name), MAX_NAME_BYTES, size)) return;
  if (version != PAGE_VERSION) return;
  name[size] = '\0';
  page = homePages::pageForSaved(indexOf(name), games::count());
}

void draw(uint8_t* frame) {
  char counter[24] = "";
  if (pages() > 1) snprintf(counter, sizeof(counter), "%d/%d", page + 1, pages());
  ui::drawBand(frame, "POCKETPLAY", counter, false);

  const int first = homePages::firstOnPage(page);
  for (int row = 0; row < homePages::rowsOnPage(page, games::count()); row++) {
    const Game& game = games::at(first + row);
    ui::drawRow(frame, rowTop(row), game.name(), game.icon(), ROW_HEIGHT);
  }

  // On the bar's left, ending at BATTERY_RIGHT at its widest.
  const battery::Reading charge = battery::read();
  ui::drawBattery(frame, ui::MARGIN, MARKS_TOP, MARKS_HEIGHT, charge.percent, charge.charging);

  if (pages() > 1) drawMarks(frame);
  drawSettings(frame);
}

bool update() { return battery::poll(); }

int onTap(int x, int y) {
  if (x >= COG_TARGET_LEFT && y >= COG_TARGET_TOP) return TAP_SETTINGS;

  // Each mark's target runs to the middle of the gap beside it, so the cluster has
  // no dead pixels, but nothing outside it answers.
  if (pages() > 1 && y >= MARKS_TOP && y < MARKS_TOP + MARKS_HEIGHT) {
    const int step = markWidth() + MARK_GAP;
    const int offset = x - marksLeft() + MARK_GAP / 2;
    if (offset < 0 || offset >= pages() * step) return TAP_NOTHING;
    return showPage(offset / step) ? TAP_PAGE : TAP_NOTHING;
  }

  const int first = homePages::firstOnPage(page);
  for (int row = 0; row < homePages::rowsOnPage(page, games::count()); row++) {
    if (ui::isInBox(x, y, ui::MARGIN, rowTop(row), ROW_WIDTH, ROW_HEIGHT)) return first + row;
  }
  return TAP_NOTHING;
}

bool onUpPressed() { return showPage(homePages::stepClamped(page, -1, games::count())); }

bool onDownPressed() { return showPage(homePages::stepClamped(page, 1, games::count())); }

bool saveFailed() { return pageSaveFailed; }

}  // namespace homeScreen
