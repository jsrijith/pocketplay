#include "about_screen.h"

#include <cstdio>

#include "core/graphics.h"
#include "core/text.h"
#include "core/version.h"
#include "ui/logo.h"
#include "ui/ui.h"

namespace aboutScreen {

namespace {

constexpr int LOGO_TOP = ui::CONTENT_TOP + 8;
constexpr int TEXT_TOP = LOGO_TOP + ui::LOGO_HEIGHT + 16;

}  // namespace

bool onTap(int x, int y) { return ui::isOnBack(x, y); }

void draw(uint8_t* frame, int gameCount) {
  ui::drawBand(frame, "ABOUT", nullptr, true);
  ui::drawLogo(frame, SCREEN_WIDTH / 2, LOGO_TOP);

  int top = TEXT_TOP;
  auto line = [&](Font font, int height, const char* text) {
    drawTextInBox(frame, font, 0, top, SCREEN_WIDTH, height, text);
    top += height;
  };
  line(Font::Size30, 42, "POCKETPLAY");
  char text[48];
  std::snprintf(text, sizeof(text), "VERSION %s", version::NUMBER);
  line(Font::Size20, 30, text);
  char commit[16];
  std::snprintf(commit, sizeof(commit), "%s", version::COMMIT);
  for (char* c = commit; *c; c++) {
    if (*c >= 'a' && *c <= 'z') *c = static_cast<char>(*c - 'a' + 'A');
  }
  std::snprintf(text, sizeof(text), "BUILD %d (%s)", version::BUILD, commit);
  line(Font::Size14, 26, text);
  top += 8;
  std::snprintf(text, sizeof(text), "%d GAMES", gameCount);
  line(Font::Size20, 30, text);
  line(Font::Size14, 26, "FOR THE SEEED RETERMINAL STICKY");
  line(Font::Size14, 26, "GITHUB.COM/JSRIJITH/POCKETPLAY");
  line(Font::Size14, 26, "EACH GAME'S CREDITS ARE IN ITS README");
}

}  // namespace aboutScreen
