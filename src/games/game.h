#pragma once

#include <cstdint>

// What every game provides. To add one, follow src/games/README.md, and follow the
// save rules in CLAUDE.md.
class Game {
 public:
  enum class TapResult {
    Nothing,     // nothing changed, so nothing is redrawn
    Redraw,      // a move: fast refresh
    RedrawFull,  // a game ending or starting: full refresh
    Leave,       // BACK: return to the home page
  };

  // Shown on the home page, and saved to reopen this game after waking. Changing
  // it means a game that was open at sleep wakes on the home page once instead.
  virtual const char* name() const = 0;

  // Called each time the game is entered. Picks up its save, or starts fresh.
  virtual void open() = 0;

  virtual TapResult onTap(int x, int y) = 0;
  virtual void draw(uint8_t* frame) = 0;

  virtual bool saveFailed() const = 0;
  virtual void retrySave() = 0;  // called before sleep if saveFailed()

 protected:
  ~Game() = default;
};
