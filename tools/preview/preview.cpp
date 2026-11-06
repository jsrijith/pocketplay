// Draws a game on the PC, so its screens can be compared with CrossPlay's before the
// device is flashed. Built by run.sh, which names the game with -DPREVIEW_GAME.
//
// Commands, run in order:
//   tap:X,Y     a tap at a screen position (480 x 800, or 800 x 480 for a landscape game); prints what it returned
//   shot:NAME   draws the game into NAME.pgm
//   row:NAME    draws the game's home-page row into NAME.pgm, to check its name and icon
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "core/graphics.h"
#include "core/storage.h"
#include "games/game.h"
#include "ui/ui.h"

Game& PREVIEW_GAME();

namespace {

uint8_t frame[EInkDisplay::DISPLAY_WIDTH_BYTES * EInkDisplay::DISPLAY_HEIGHT];

// The framebuffer is landscape, so each pixel is read back the way setPixel()
// wrote it in the current orientation.
bool writePgm(const std::string& path) {
  FILE* file = fopen(path.c_str(), "wb");
  if (file == nullptr) return false;
  const bool landscape = orientation() == Orientation::Landscape;
  const int width = screenWidth();
  const int height = screenHeight();
  fprintf(file, "P5 %d %d 255\n", width, height);
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      const int panelX = landscape ? x : y;
      const int panelY = landscape ? y : SCREEN_WIDTH - 1 - x;
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
  storage::begin();
  Game& game = PREVIEW_GAME();
  game.open();
  // As the shell does: the game's own orientation for its taps and screens.
  const Orientation gameOrientation = game.landscape() ? Orientation::Landscape : Orientation::Portrait;
  setOrientation(gameOrientation);

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
        setOrientation(Orientation::Portrait);  // the home page is always portrait
        ui::drawRow(frame, ui::CONTENT_TOP, game.name(), game.icon());
      } else {
        game.draw(frame);
      }
      const bool written = writePgm(outDir + "/" + name + ".pgm");
      setOrientation(gameOrientation);
      if (!written) {
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
