// Draws a game on the PC, so its screens can be compared with CrossPlay's before the
// device is flashed. Built by run.sh, which names the game with -DPREVIEW_GAME, or an
// app with -DPREVIEW_APP. An app is wrapped as a game, and the commands it has no
// calls for (press:, hold:, release, swipe:) stop with an error.
//
// Commands, run in order:
//   tap:X,Y     a tap at a screen position (480 x 800, or 800 x 480 for a landscape game); prints what it returned
//   shot:NAME   draws the game into NAME.pgm
//   row:NAME    draws the game's home-page row into NAME.pgm, to check its name and icon
//   warning:NAME  draws the game with the shell's longest warning over it, to check it clears
//   update      runs the game's update() as the loop would, until it's no longer busy()
//   press:X,Y   a finger down, hold:X,Y the same finger held, release its lift: the calls a
//               game whose usesHold() is true gets. A hold's lift taps nothing, as on the device.
//   swipe:X1,Y1,X2,Y2  a finger drawn from one screen position to another; prints what it returned
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "core/graphics.h"
#include "core/storage.h"
#include "games/game.h"
#include "ui/ui.h"

#ifdef PREVIEW_APP
#include "apps/app.h"

App& PREVIEW_APP();

namespace {

// Forwards to the app the calls it has, so the commands below work on it unchanged.
class AppAsGame final : public Game {
 public:
  explicit AppAsGame(App& app) : app_(app) {}
  const char* name() const override { return app_.name(); }
  const uint8_t* icon() const override { return app_.icon(); }
  void open() override { app_.open(); }
  TapResult onTap(int x, int y) override { return convert(app_.onTap(x, y)); }
  void draw(uint8_t* frame) override { app_.draw(frame); }
  bool saveFailed() const override { return app_.saveFailed(); }
  void retrySave() override { app_.retrySave(); }
  TapResult onUpPressed() override { return convert(app_.onUpPressed()); }
  TapResult onDownPressed() override { return convert(app_.onDownPressed()); }
  bool update() override { return app_.update(); }
  bool busy() const override { return app_.busy(); }
  void onWifiClosed() override { app_.onWifiClosed(); }

 private:
  static TapResult convert(App::TapResult result) {
    switch (result) {
      case App::TapResult::Nothing:
        return TapResult::Nothing;
      case App::TapResult::Redraw:
        return TapResult::Redraw;
      case App::TapResult::RedrawFull:
        return TapResult::RedrawFull;
      case App::TapResult::Leave:
        return TapResult::Leave;
      case App::TapResult::OpenWifi:
        return TapResult::OpenWifi;
    }
    return TapResult::Nothing;
  }
  App& app_;
};

constexpr bool IS_APP = true;

Game& previewed() {
  static AppAsGame game(PREVIEW_APP());
  return game;
}

}  // namespace
#else
Game& PREVIEW_GAME();

namespace {
constexpr bool IS_APP = false;
Game& previewed() { return PREVIEW_GAME(); }
}  // namespace
#endif

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
  Game& game = previewed();
  game.open();
  // As the shell does: the game's own orientation for its taps and screens.
  const Orientation gameOrientation = game.landscape() ? Orientation::Landscape : Orientation::Portrait;
  setOrientation(gameOrientation);

  for (int i = 2; i < argc; i++) {
    const std::string command = argv[i];
    if (IS_APP && (command.rfind("press:", 0) == 0 || command.rfind("hold:", 0) == 0 || command == "release" ||
                   command.rfind("swipe:", 0) == 0)) {
      fprintf(stderr, "%s: apps don't get holds or swipes\n", command.c_str());
      return 2;
    }
    if (command.rfind("press:", 0) == 0 || command.rfind("hold:", 0) == 0) {
      const bool hold = command[0] == 'h';
      int x = 0;
      int y = 0;
      if (sscanf(command.c_str() + command.find(':') + 1, "%d,%d", &x, &y) != 2) {
        fprintf(stderr, "bad %s\n", command.c_str());
        return 2;
      }
      if (!game.usesHold()) {
        fprintf(stderr, "%s: this game doesn't use holds\n", command.c_str());
        return 2;
      }
      printf("%s -> %s\n", command.c_str(), resultName(hold ? game.onHold(x, y) : game.onPress(x, y)));
    } else if (command == "update") {
      // At least once, then for as long as the game says it has work going.
      bool redraw = false;
      int passes = 0;
      do {
        redraw = game.update() || redraw;
        passes++;
      } while (game.busy() && passes < 100000);
      printf("update -> %s after %d pass%s\n", redraw ? "Redraw" : "Nothing", passes, passes == 1 ? "" : "es");
    } else if (command == "release") {
      printf("release -> %s\n", resultName(game.onRelease()));
    } else if (command == "up" || command == "down") {
      printf("%s -> %s\n", command.c_str(), resultName(command == "up" ? game.onUpPressed() : game.onDownPressed()));
    } else if (command.rfind("swipe:", 0) == 0) {
      int from[2] = {};
      int to[2] = {};
      if (sscanf(command.c_str() + 6, "%d,%d,%d,%d", &from[0], &from[1], &to[0], &to[1]) != 4) {
        fprintf(stderr, "bad swipe: %s\n", command.c_str());
        return 2;
      }
      printf("%s -> %s\n", command.c_str(), resultName(game.onSwipe(from[0], from[1], to[0], to[1])));
    } else if (command.rfind("tap:", 0) == 0) {
      int x = 0;
      int y = 0;
      if (sscanf(command.c_str() + 4, "%d,%d", &x, &y) != 2) {
        fprintf(stderr, "bad tap: %s\n", command.c_str());
        return 2;
      }
      printf("%s -> %s\n", command.c_str(), resultName(game.onTap(x, y)));
    } else if (command.rfind("shot:", 0) == 0 || command.rfind("row:", 0) == 0 || command.rfind("warning:", 0) == 0) {
      const bool row = command[0] == 'r';
      const bool warning = command[0] == 'w';
      const std::string name = command.substr(command.find(':') + 1);
      memset(frame, 0xFF, sizeof(frame));
      if (row) {
        setOrientation(Orientation::Portrait);  // the home page is always portrait
        ui::drawRow(frame, ui::CONTENT_TOP, game.name(), game.icon());
      } else {
        game.draw(frame);
        if (warning) ui::drawWarning(frame, "SAVE FAILED");
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
