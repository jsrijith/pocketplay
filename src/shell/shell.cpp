#include "shell.h"

#include <cstring>

#include "core/network.h"
#include "core/storage.h"
#include "games/games.h"
#include "home_screen.h"
#include "settings/settings_screen.h"
#include "settings/wifi_screen.h"
#include "ui/ui.h"

namespace shell {

namespace {

// Holds the open game's name rather than its place in the list, so adding a game
// later can't reopen the wrong one.
constexpr const char* SESSION_FILE = "session";
constexpr uint8_t SESSION_VERSION = 1;
constexpr size_t MAX_NAME_BYTES = 32;

enum class Screen { Home, Settings, Wifi, Playing };

Screen screen = Screen::Home;
Game* currentGame = nullptr;
bool hasTouch = true;
bool sessionSaveFailed = false;

// Without this file the device wakes on the home page, with the game's own save
// intact, so a failure here only costs a tap.
void saveSession(const char* name) {
  if (!storage::ready()) return;
  const size_t length = strlen(name);
  sessionSaveFailed = !storage::save(SESSION_FILE, SESSION_VERSION, reinterpret_cast<const uint8_t*>(name), length);
}

void clearSession() {
  if (!storage::ready()) return;
  sessionSaveFailed = !storage::erase(SESSION_FILE);
}

void loadSession() {
  if (!storage::ready()) return;

  char name[MAX_NAME_BYTES + 1];
  uint8_t version = 0;
  size_t size = 0;
  if (!storage::load(SESSION_FILE, version, reinterpret_cast<uint8_t*>(name), MAX_NAME_BYTES, size)) return;
  if (version != SESSION_VERSION) return;
  name[size] = '\0';

  currentGame = games::find(name);
  if (currentGame != nullptr) {
    currentGame->open();
    screen = Screen::Playing;
  }
}

Redraw tapHome(int x, int y) {
  const int tapped = homeScreen::onTap(x, y);
  if (tapped == homeScreen::TAP_NOTHING) return Redraw::None;

  if (tapped == homeScreen::TAP_SETTINGS) {
    screen = Screen::Settings;
    return Redraw::Fast;
  }

  currentGame = &games::at(tapped);
  currentGame->open();
  saveSession(currentGame->name());
  screen = Screen::Playing;
  return Redraw::Fast;
}

Redraw tapSettings(int x, int y) {
  switch (settingsScreen::onTap(x, y)) {
    case settingsScreen::Tap::Nothing:
      return Redraw::None;
    case settingsScreen::Tap::Changed:
      return Redraw::Fast;
    case settingsScreen::Tap::Back:
      screen = Screen::Home;
      return Redraw::Fast;
    case settingsScreen::Tap::OpenWifi:
      wifiScreen::open();
      screen = Screen::Wifi;
      return Redraw::Fast;
  }
  return Redraw::None;
}

Redraw tapWifi(int x, int y) {
  const bool changed = wifiScreen::onTap(x, y);
  if (!wifiScreen::isOpen()) screen = Screen::Settings;
  return changed ? Redraw::Fast : Redraw::None;
}

Redraw tapGame(int x, int y) {
  switch (currentGame->onTap(x, y)) {
    case Game::TapResult::Nothing:
      return Redraw::None;
    case Game::TapResult::Redraw:
      return Redraw::Fast;
    case Game::TapResult::RedrawFull:
      return Redraw::Full;
    case Game::TapResult::Leave:
      clearSession();
      screen = Screen::Home;
      return Redraw::Fast;
  }
  return Redraw::None;
}

const char* warningText() {
  if (!hasTouch) return "NO TOUCH";
  if (!storage::ready()) return "NO CARD";

  const bool gameFailed = currentGame != nullptr && currentGame->saveFailed();
  if (gameFailed || settingsScreen::saveFailed() || sessionSaveFailed) return "SAVE FAILED";
  return nullptr;
}

}  // namespace

void begin(bool touchWorks) {
  hasTouch = touchWorks;
  settingsScreen::begin();
  wifiScreen::begin();
  loadSession();
}

// Only the screens that show the network redraw for it, so a board never blinks
// because the connection changed.
Redraw update() {
  const bool networkChanged = wifiScreen::update();
  if (!networkChanged) return Redraw::None;
  return screen == Screen::Settings || screen == Screen::Wifi ? Redraw::Fast : Redraw::None;
}

Redraw onTap(int x, int y) {
  switch (screen) {
    case Screen::Home:
      return tapHome(x, y);
    case Screen::Settings:
      return tapSettings(x, y);
    case Screen::Wifi:
      return tapWifi(x, y);
    case Screen::Playing:
      return tapGame(x, y);
  }
  return Redraw::None;
}

Redraw onUpPressed() { return screen == Screen::Wifi && wifiScreen::onUpPressed() ? Redraw::Fast : Redraw::None; }

Redraw onDownPressed() { return screen == Screen::Wifi && wifiScreen::onDownPressed() ? Redraw::Fast : Redraw::None; }

void draw(uint8_t* frame) {
  switch (screen) {
    case Screen::Home:
      homeScreen::draw(frame);
      break;
    case Screen::Settings:
      settingsScreen::draw(frame);
      break;
    case Screen::Wifi:
      wifiScreen::draw(frame);
      return;  // it reports its own problems, and its keyboard fills this space
    case Screen::Playing:
      currentGame->draw(frame);
      break;
  }

  if (const char* warning = warningText()) ui::drawWarning(frame, warning);
}

uint32_t idleSleepMs() { return settingsScreen::idleSleepMinutes() * 60UL * 1000UL; }

void beforeSleep() {
  // Otherwise the modem's power domain could stay on through deep sleep.
  network::turnOff();

  // Every change is saved as it happens, so this only retries one that failed.
  if (settingsScreen::saveFailed()) settingsScreen::retrySave();
  if (currentGame != nullptr && currentGame->saveFailed()) currentGame->retrySave();
}

}  // namespace shell
