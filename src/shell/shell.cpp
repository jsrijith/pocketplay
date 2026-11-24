#include "shell.h"

#include <cstring>

#include "apps/apps.h"
#include "apps_screen.h"
#include "core/download.h"
#include "core/graphics.h"
#include "core/link/radio.h"
#include "core/network.h"
#include "core/storage.h"
#include "games/games.h"
#include "home_screen.h"
#include "settings/about_screen.h"
#include "settings/settings_screen.h"
#include "settings/timezone_screen.h"
#include "settings/wifi_screen.h"
#include "ui/ui.h"

namespace shell {

namespace {

// Holds the open game's or app's name rather than its place in a list, so adding
// one later can't reopen the wrong one. Version 2 puts a kind byte before the name;
// version 1 held a game's name alone, and still loads as one.
constexpr const char* SESSION_FILE = "session";
constexpr uint8_t SESSION_VERSION = 2;
constexpr uint8_t SESSION_VERSION_GAME_ONLY = 1;
constexpr uint8_t KIND_GAME = 0;
constexpr uint8_t KIND_APP = 1;
constexpr size_t MAX_NAME_BYTES = 32;

enum class Screen { Home, Settings, Wifi, TimeZone, About, Playing, Apps, UsingApp };

Screen screen = Screen::Home;
Game* currentGame = nullptr;
App* currentApp = nullptr;
bool hasTouch = true;
bool sessionSaveFailed = false;
// Who opened the Wi-Fi screen, so closing it goes back there. A game or an app gets
// onWifiClosed(), and is returned to as soon as the network joins.
enum class WifiOpener { Settings, Game, App };
WifiOpener wifiOpener = WifiOpener::Settings;

// Without this file the device wakes on the home page, with the game's own save
// intact, so a failure here only costs a tap.
void saveSession(uint8_t kind, const char* name) {
  if (!storage::ready()) return;
  uint8_t data[1 + MAX_NAME_BYTES];
  size_t length = strlen(name);
  if (length > MAX_NAME_BYTES) length = MAX_NAME_BYTES;
  data[0] = kind;
  memcpy(data + 1, name, length);
  sessionSaveFailed = !storage::save(SESSION_FILE, SESSION_VERSION, data, 1 + length);
}

void clearSession() {
  if (!storage::ready()) return;
  sessionSaveFailed = !storage::erase(SESSION_FILE);
}

void loadSession() {
  if (!storage::ready()) return;

  uint8_t data[1 + MAX_NAME_BYTES];
  uint8_t version = 0;
  size_t size = 0;
  if (!storage::load(SESSION_FILE, version, data, sizeof(data), size)) return;

  uint8_t kind = KIND_GAME;
  const uint8_t* start = data;
  if (version == SESSION_VERSION) {
    if (size < 1) return;
    kind = data[0];
    start = data + 1;
    size -= 1;
  } else if (version != SESSION_VERSION_GAME_ONLY || size > MAX_NAME_BYTES) {
    return;
  }
  char name[MAX_NAME_BYTES + 1];
  memcpy(name, start, size);
  name[size] = '\0';

  if (kind == KIND_GAME) {
    currentGame = games::find(name);
    if (currentGame != nullptr) {
      currentGame->open();
      screen = Screen::Playing;
    }
  } else if (kind == KIND_APP) {
    currentApp = apps::find(name);
    if (currentApp != nullptr) {
      currentApp->open();
      screen = Screen::UsingApp;
    }
  }
}

Redraw tapHome(int x, int y) {
  const int tapped = homeScreen::onTap(x, y);
  if (tapped == homeScreen::TAP_NOTHING) return Redraw::None;
  if (tapped == homeScreen::TAP_PAGE) return Redraw::Fast;

  if (tapped == homeScreen::TAP_SETTINGS) {
    screen = Screen::Settings;
    return Redraw::Fast;
  }
  if (tapped == homeScreen::TAP_APPS) {
    screen = Screen::Apps;
    return Redraw::Fast;
  }

  currentGame = &games::at(tapped);
  currentGame->open();
  saveSession(KIND_GAME, currentGame->name());
  screen = Screen::Playing;
  return Redraw::Fast;
}

Redraw tapApps(int x, int y) {
  const int tapped = appsScreen::onTap(x, y);
  if (tapped == appsScreen::TAP_NOTHING) return Redraw::None;
  if (tapped == appsScreen::TAP_BACK) {
    screen = Screen::Home;
    return Redraw::Fast;
  }

  currentApp = &apps::at(tapped);
  currentApp->open();
  saveSession(KIND_APP, currentApp->name());
  screen = Screen::UsingApp;
  return Redraw::Fast;
}

// BACK from an app returns to the APPS page it was opened from.
Redraw applyAppResult(App::TapResult result) {
  switch (result) {
    case App::TapResult::Nothing:
      return Redraw::None;
    case App::TapResult::Redraw:
      return Redraw::Fast;
    case App::TapResult::RedrawFull:
      return Redraw::Full;
    case App::TapResult::Leave:
      clearSession();
      screen = Screen::Apps;
      return Redraw::Fast;
    case App::TapResult::OpenWifi:
      wifiOpener = WifiOpener::App;
      wifiScreen::open();
      screen = Screen::Wifi;
      return Redraw::Fast;
  }
  return Redraw::None;
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
      wifiOpener = WifiOpener::Settings;
      wifiScreen::open();
      screen = Screen::Wifi;
      return Redraw::Fast;
    case settingsScreen::Tap::OpenTimeZone:
      timezoneScreen::open();
      screen = Screen::TimeZone;
      return Redraw::Fast;
    case settingsScreen::Tap::OpenAbout:
      screen = Screen::About;
      return Redraw::Fast;
  }
  return Redraw::None;
}

// Back to whoever opened the Wi-Fi screen.
void leaveWifi() {
  const WifiOpener opener = wifiOpener;
  wifiOpener = WifiOpener::Settings;
  switch (opener) {
    case WifiOpener::Settings:
      screen = Screen::Settings;
      break;
    case WifiOpener::Game:
      screen = Screen::Playing;
      currentGame->onWifiClosed();
      break;
    case WifiOpener::App:
      screen = Screen::UsingApp;
      currentApp->onWifiClosed();
      break;
  }
}

Redraw tapWifi(int x, int y) {
  const bool changed = wifiScreen::onTap(x, y);
  if (!wifiScreen::isOpen()) leaveWifi();
  return changed ? Redraw::Fast : Redraw::None;
}

Redraw tapTimeZone(int x, int y) {
  const bool changed = timezoneScreen::onTap(x, y);
  if (!timezoneScreen::isOpen()) screen = Screen::Settings;
  return changed ? Redraw::Fast : Redraw::None;
}

Redraw tapAbout(int x, int y) {
  if (!aboutScreen::onTap(x, y)) return Redraw::None;
  screen = Screen::Settings;
  return Redraw::Fast;
}

Redraw applyGameResult(Game::TapResult result) {
  switch (result) {
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
    case Game::TapResult::OpenWifi:
      wifiOpener = WifiOpener::Game;
      wifiScreen::open();
      screen = Screen::Wifi;
      return Redraw::Fast;
  }
  return Redraw::None;
}

Redraw tapGame(int x, int y) { return applyGameResult(currentGame->onTap(x, y)); }

const char* warningText() {
  if (!hasTouch) return "NO TOUCH";
  if (!storage::ready()) return "NO CARD";

  const bool gameFailed = currentGame != nullptr && currentGame->saveFailed();
  const bool appFailed = currentApp != nullptr && currentApp->saveFailed();
  if (gameFailed || appFailed || settingsScreen::saveFailed() || sessionSaveFailed || homeScreen::saveFailed()) return "SAVE FAILED";
  return nullptr;
}

}  // namespace

void begin(bool touchWorks) {
  hasTouch = touchWorks;
  settingsScreen::begin();
  wifiScreen::begin();
  homeScreen::begin();
  loadSession();
}

// Only the screens that show the network redraw for it, so a board never blinks
// because the connection changed.
Redraw update() {
  const bool networkChanged = wifiScreen::update();

  // A game or an app asked for Wi-Fi, so it goes back as soon as it has it.
  if (screen == Screen::Wifi && wifiOpener != WifiOpener::Settings && network::state() == network::State::Connected) {
    wifiScreen::close();
    leaveWifi();
    return Redraw::Fast;
  }
  if (screen == Screen::Playing && currentGame->update()) return Redraw::Fast;
  if (screen == Screen::UsingApp && currentApp->update()) return Redraw::Fast;
  if (screen == Screen::Home && homeScreen::update()) return Redraw::Fast;

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
    case Screen::TimeZone:
      return tapTimeZone(x, y);
    case Screen::About:
      return tapAbout(x, y);
    case Screen::Playing:
      return tapGame(x, y);
    case Screen::Apps:
      return tapApps(x, y);
    case Screen::UsingApp:
      return applyAppResult(currentApp->onTap(x, y));
  }
  return Redraw::None;
}

bool usesHold() { return screen == Screen::Playing && currentGame->usesHold(); }

Redraw onPress(int x, int y) { return usesHold() ? applyGameResult(currentGame->onPress(x, y)) : Redraw::None; }
Redraw onHold(int x, int y) { return usesHold() ? applyGameResult(currentGame->onHold(x, y)) : Redraw::None; }
Redraw onRelease() { return usesHold() ? applyGameResult(currentGame->onRelease()) : Redraw::None; }

Redraw onUpPressed() {
  if (screen == Screen::Home) return homeScreen::onUpPressed() ? Redraw::Fast : Redraw::None;
  if (screen == Screen::Wifi) return wifiScreen::onUpPressed() ? Redraw::Fast : Redraw::None;
  if (screen == Screen::TimeZone) return timezoneScreen::onUpPressed() ? Redraw::Fast : Redraw::None;
  if (screen == Screen::Playing) return applyGameResult(currentGame->onUpPressed());
  if (screen == Screen::UsingApp) return applyAppResult(currentApp->onUpPressed());
  return Redraw::None;
}

Redraw onDownPressed() {
  if (screen == Screen::Home) return homeScreen::onDownPressed() ? Redraw::Fast : Redraw::None;
  if (screen == Screen::Wifi) return wifiScreen::onDownPressed() ? Redraw::Fast : Redraw::None;
  if (screen == Screen::TimeZone) return timezoneScreen::onDownPressed() ? Redraw::Fast : Redraw::None;
  if (screen == Screen::Playing) return applyGameResult(currentGame->onDownPressed());
  if (screen == Screen::UsingApp) return applyAppResult(currentApp->onDownPressed());
  return Redraw::None;
}

Redraw onSwipe(int fromX, int fromY, int toX, int toY) {
  if (screen != Screen::Playing) return Redraw::None;
  return applyGameResult(currentGame->onSwipe(fromX, fromY, toX, toY));
}

bool landscape() { return screen == Screen::Playing && currentGame->landscape(); }

void draw(uint8_t* frame) {
  setOrientation(landscape() ? Orientation::Landscape : Orientation::Portrait);
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
    case Screen::TimeZone:
      timezoneScreen::draw(frame);
      break;
    case Screen::About:
      aboutScreen::draw(frame, games::count());
      break;
    case Screen::Playing:
      currentGame->draw(frame);
      break;
    case Screen::Apps:
      appsScreen::draw(frame);
      break;
    case Screen::UsingApp:
      currentApp->draw(frame);
      break;
  }

  if (const char* warning = warningText()) ui::drawWarning(frame, warning);
}

bool busy() {
  return (screen == Screen::Playing && currentGame->busy()) || (screen == Screen::UsingApp && currentApp->busy());
}

int updatesPerCleanup() { return settingsScreen::updatesPerCleanup(); }

uint32_t idleSleepMs() { return settingsScreen::idleSleepMinutes() * 60UL * 1000UL; }

void beforeSleep() {
  // First, while the radio is still up, so the other device is told we're going
  // rather than waiting out the silence. Does nothing outside a match.
  linkplay::stopForSleep();

  // Before the radio, which a running download is using.
  download::stopForSleep();

  // Otherwise the modem's power domain could stay on through deep sleep.
  network::turnOff();

  // Every change is saved as it happens, so this only retries one that failed.
  if (settingsScreen::saveFailed()) settingsScreen::retrySave();
  if (currentGame != nullptr && currentGame->saveFailed()) currentGame->retrySave();
  if (currentApp != nullptr && currentApp->saveFailed()) currentApp->retrySave();
}

}  // namespace shell
