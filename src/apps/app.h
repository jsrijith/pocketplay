#pragma once

#include <cstdint>

// What every app provides: a tool rather than a game, reached from the APPS page. It
// is a smaller contract than Game's: taps, the Up and Down buttons, Wi-Fi, and work
// between taps. Add to it when an app needs more, not before. To add an app, follow
// src/apps/README.md, and the save rules in CLAUDE.md.
class App {
 public:
  enum class TapResult {
    Nothing,     // nothing changed, so nothing is redrawn
    Redraw,      // a change: fast refresh
    RedrawFull,  // a big change, such as a new page: full refresh
    Leave,       // BACK: return to the APPS page
    OpenWifi,    // show the Wi-Fi screen, then come back and call onWifiClosed()
  };

  // Shown on the APPS page, and saved to reopen this app after waking. Changing it
  // means an app that was open at sleep wakes on the home page once instead.
  virtual const char* name() const = 0;

  // The APPS page's icon for this app: 32 x 32 in ui::ICON_SIZE's format, or nullptr
  // for none.
  virtual const uint8_t* icon() const { return nullptr; }

  // Called each time the app is entered. Picks up its save, or starts fresh.
  virtual void open() = 0;

  virtual TapResult onTap(int x, int y) = 0;
  virtual void draw(uint8_t* frame) = 0;

  virtual bool saveFailed() const = 0;
  virtual void retrySave() = 0;  // called before sleep if saveFailed()

  // The Up and Down buttons, while the app is open. An app that doesn't use them
  // ignores them.
  virtual TapResult onUpPressed() { return TapResult::Nothing; }
  virtual TapResult onDownPressed() { return TapResult::Nothing; }

  // For an app with work going on between taps, such as a download. Called every
  // loop while it's on screen, and returns true if it needs redrawing.
  virtual bool update() { return false; }

  // True stops the idle timer, for work that a sleep would throw away.
  virtual bool busy() const { return false; }

  // Back from OpenWifi, which closes by itself once it joins. network::state() says
  // whether it did.
  virtual void onWifiClosed() {}

 protected:
  ~App() = default;
};
