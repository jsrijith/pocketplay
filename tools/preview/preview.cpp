// Draws a game on the PC, so its screens can be compared with CrossPlay's before the
// device is flashed. Built by run.sh, which names the game with -DPREVIEW_GAME.
//
// Commands, run in order:
//   tap:X,Y     a tap at a screen position (portrait, 480 x 800); prints what it returned
//   shot:NAME   draws the game into NAME.pgm
//   row:NAME    draws the game's home-page row into NAME.pgm, to check its name and icon
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "core/graphics.h"
#include "games/game.h"
#include "ui/ui.h"

Game& PREVIEW_GAME();

namespace {

uint8_t frame[EInkDisplay::DISPLAY_WIDTH_BYTES * EInkDisplay::DISPLAY_HEIGHT];

// The framebuffer is landscape, so each portrait pixel is read back the way
// setPixel() wrote it.
bool writePgm(const std::string& path) {
  FILE* file = fopen(path.c_str(), "wb");
  if (file == nullptr) return false;
  fprintf(file, "P5 %d %d 255\n", SCREEN_WIDTH, SCREEN_HEIGHT);
  for (int y = 0; y < SCREEN_HEIGHT; y++) {
    for (int x = 0; x < SCREEN_WIDTH; x++) {
      const int panelX = y;
      const int panelY = SCREEN_WIDTH - 1 - x;
      const bool white = (frame[panelY * EInkDisplay::DISPLAY_WIDTH_BYTES + panelX / 8] & (0x80 >> (panelX % 8))) != 0;
      fputc(white ? 255 : 0, file);
    }
  }
  fclose(file);
  return true;
}

const char* resultName(Game::TapResult result) {
  switch (result) {
    case Game::TapResult::Nothing:
      return "Nothing";
    case Game::TapResult::Redraw:
      return "Redraw";
    case Game::TapResult::RedrawFull:
      return "RedrawFull";
    case Game::TapResult::Leave:
      return "Leave";
    case Game::TapResult::OpenWifi:
      return "OpenWifi";
  }
  return "?";
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: preview OUT_DIR COMMAND...\n");
    return 2;
  }
  const std::string outDir = argv[1];
  Game& game = PREVIEW_GAME();
  game.open();

  for (int i = 2; i < argc; i++) {
    const std::string command = argv[i];
    if (command.rfind("tap:", 0) == 0) {
      int x = 0;
      int y = 0;
      if (sscanf(command.c_str() + 4, "%d,%d", &x, &y) != 2) {
        fprintf(stderr, "bad tap: %s\n", command.c_str());
        return 2;
      }
      printf("%s -> %s\n", command.c_str(), resultName(game.onTap(x, y)));
    } else if (command.rfind("shot:", 0) == 0 || command.rfind("row:", 0) == 0) {
      const bool row = command[0] == 'r';
      const std::string name = command.substr(row ? 4 : 5);
      memset(frame, 0xFF, sizeof(frame));
      if (row) {
        ui::drawRow(frame, ui::CONTENT_TOP, game.name(), game.icon());
      } else {
        game.draw(frame);
      }
      if (!writePgm(outDir + "/" + name + ".pgm")) {
        fprintf(stderr, "can't write %s\n", name.c_str());
        return 1;
      }
      printf("%s\n", command.c_str());
    } else {
      fprintf(stderr, "unknown command: %s\n", command.c_str());
      return 2;
    }
  }
  return 0;
}
