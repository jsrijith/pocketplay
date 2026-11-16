#include "apps_screen.h"

#include "apps/apps.h"
#include "core/graphics.h"
#include "core/text.h"
#include "ui/ui.h"

namespace appsScreen {

namespace {

constexpr int ROW_WIDTH = SCREEN_WIDTH - 2 * ui::MARGIN;

// As many rows as fit above the bottom margin. An app past this isn't shown, so
// the page needs paging, as the home page has, before the list grows that long.
constexpr int BOTTOM_MARGIN = 16;
constexpr int ROWS = (SCREEN_HEIGHT - BOTTOM_MARGIN - ui::CONTENT_TOP + ui::ROW_GAP) / (ui::ROW_HEIGHT + ui::ROW_GAP);

int rowTop(int row) { return ui::CONTENT_TOP + row * (ui::ROW_HEIGHT + ui::ROW_GAP); }

int rowsShown() { return apps::count() < ROWS ? apps::count() : ROWS; }

}  // namespace

void draw(uint8_t* frame) {
  ui::drawBand(frame, "APPS", nullptr, true);
  if (apps::count() == 0) {
    drawTextInBox(frame, Font::Size20, ui::MARGIN, ui::CONTENT_TOP, ROW_WIDTH, ui::ROW_HEIGHT, "NO APPS YET");
    return;
  }
  for (int row = 0; row < rowsShown(); row++) {
    const App& app = apps::at(row);
    ui::drawRow(frame, rowTop(row), app.name(), app.icon());
  }
}

int onTap(int x, int y) {
  if (ui::isOnBack(x, y)) return TAP_BACK;
  for (int row = 0; row < rowsShown(); row++) {
    if (ui::isInBox(x, y, ui::MARGIN, rowTop(row), ROW_WIDTH, ui::ROW_HEIGHT)) return row;
  }
  return TAP_NOTHING;
}

}  // namespace appsScreen
