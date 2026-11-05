#include "timezone_screen.h"

#include <cstdio>

#include "core/clock.h"
#include "core/graphics.h"
#include "core/text.h"
#include "settings_screen.h"
#include "ui/ui.h"

namespace timezoneScreen {

namespace {

constexpr int ROWS_TOP = ui::CONTENT_TOP;
constexpr int ROW_HEIGHT = 72;
constexpr int ROWS_PER_PAGE = 8;
constexpr int PAGE_COUNT = (wallClock::ZONE_COUNT + ROWS_PER_PAGE - 1) / ROWS_PER_PAGE;
constexpr int OFFSET_WIDTH = 110;

// The list is long, so paging works by touch as well as with UP and DOWN.
constexpr int BUTTONS_TOP = ROWS_TOP + ROWS_PER_PAGE * ROW_HEIGHT + ui::GAP;
constexpr int BUTTON_HEIGHT = SCREEN_HEIGHT - ui::MARGIN - BUTTONS_TOP;
constexpr int BUTTON_WIDTH = 130;
constexpr int NEXT_LEFT = SCREEN_WIDTH - ui::MARGIN - BUTTON_WIDTH;

bool screenOpen = false;
int page = 0;

bool previousPage() {
  if (page == 0) return false;
  page--;
  return true;
}

bool nextPage() {
  if (page + 1 >= PAGE_COUNT) return false;
  page++;
  return true;
}

void drawRow(uint8_t* frame, int index, int top) {
  const bool current = index == settingsScreen::timeZone();
  if (current) fillRect(frame, ui::MARGIN, top, SCREEN_WIDTH - 2 * ui::MARGIN, ROW_HEIGHT - 1, true);

  const int textLeft = ui::MARGIN + 8;
  const int offsetLeft = SCREEN_WIDTH - ui::MARGIN - 8 - OFFSET_WIDTH;
  const int baseline = top + (ROW_HEIGHT + capHeight(Font::Size20)) / 2;

  char name[48];
  fitText(Font::Size20, wallClock::zone(index).name, offsetLeft - textLeft - 8, name, sizeof(name));
  drawText(frame, Font::Size20, textLeft, baseline, name, Align::Left, !current);

  char offset[12];
  wallClock::formatOffset(index, offset, sizeof(offset));
  drawText(frame, Font::Size14, SCREEN_WIDTH - ui::MARGIN - 8, top + (ROW_HEIGHT + capHeight(Font::Size14)) / 2, offset, Align::Right, !current);

  fillRect(frame, ui::MARGIN, top + ROW_HEIGHT - 1, SCREEN_WIDTH - 2 * ui::MARGIN, 1, true);
}

}  // namespace

void open() {
  screenOpen = true;
  page = settingsScreen::timeZone() / ROWS_PER_PAGE;
}

bool isOpen() { return screenOpen; }

bool onTap(int x, int y) {
  if (ui::isOnBack(x, y)) {
    screenOpen = false;
    return true;
  }

  if (ui::isInBox(x, y, ui::MARGIN, BUTTONS_TOP, BUTTON_WIDTH, BUTTON_HEIGHT)) return previousPage();
  if (ui::isInBox(x, y, NEXT_LEFT, BUTTONS_TOP, BUTTON_WIDTH, BUTTON_HEIGHT)) return nextPage();

  if (y < ROWS_TOP || y >= ROWS_TOP + ROWS_PER_PAGE * ROW_HEIGHT) return false;
  const int index = page * ROWS_PER_PAGE + (y - ROWS_TOP) / ROW_HEIGHT;
  if (index >= wallClock::ZONE_COUNT) return false;

  settingsScreen::setTimeZone(index);
  screenOpen = false;
  return true;
}

bool onUpPressed() { return previousPage(); }

bool onDownPressed() { return nextPage(); }

void draw(uint8_t* frame) {
  ui::drawHeader(frame, "TIME ZONE");

  for (int i = 0; i < ROWS_PER_PAGE; i++) {
    const int index = page * ROWS_PER_PAGE + i;
    if (index >= wallClock::ZONE_COUNT) break;
    drawRow(frame, index, ROWS_TOP + i * ROW_HEIGHT);
  }

  ui::drawButton(frame, ui::MARGIN, BUTTONS_TOP, BUTTON_WIDTH, BUTTON_HEIGHT, "PREV");
  ui::drawButton(frame, NEXT_LEFT, BUTTONS_TOP, BUTTON_WIDTH, BUTTON_HEIGHT, "NEXT");

  char label[16];
  snprintf(label, sizeof(label), "%d / %d", page + 1, PAGE_COUNT);
  drawTextInBox(frame, Font::Size14, 0, BUTTONS_TOP, SCREEN_WIDTH, BUTTON_HEIGHT, label);
}

}  // namespace timezoneScreen
