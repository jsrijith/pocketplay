#include "home_screen.h"

#include "core/graphics.h"
#include "core/text.h"
#include "games/games.h"
#include "ui/ui.h"

namespace homeScreen {

namespace {

constexpr int ROW_WIDTH = SCREEN_WIDTH - 2 * ui::MARGIN;
constexpr int ROW_HEIGHT = 84;
constexpr int SETTINGS_TOP = SCREEN_HEIGHT - ui::MARGIN - ROW_HEIGHT;

int rowTop(int index) { return ui::CONTENT_TOP + index * (ROW_HEIGHT + ui::GAP); }

}  // namespace

void draw(uint8_t* frame) {
  drawTextInBox(frame, Font::Size30, 0, ui::HEADER_TOP, SCREEN_WIDTH, ui::HEADER_BUTTON_HEIGHT, "POCKETPLAY");

  for (int i = 0; i < games::count(); i++) {
    ui::drawButton(frame, ui::MARGIN, rowTop(i), ROW_WIDTH, ROW_HEIGHT, games::at(i).name());
  }
  ui::drawButton(frame, ui::MARGIN, SETTINGS_TOP, ROW_WIDTH, ROW_HEIGHT, "SETTINGS");
}

int onTap(int x, int y) {
  if (ui::isInBox(x, y, ui::MARGIN, SETTINGS_TOP, ROW_WIDTH, ROW_HEIGHT)) return TAP_SETTINGS;

  for (int i = 0; i < games::count(); i++) {
    if (ui::isInBox(x, y, ui::MARGIN, rowTop(i), ROW_WIDTH, ROW_HEIGHT)) return i;
  }
  return TAP_NOTHING;
}

}  // namespace homeScreen
