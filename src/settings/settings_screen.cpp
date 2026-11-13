#include "settings_screen.h"

#include <Arduino.h>

#include <cstdio>

#include "core/clock.h"
#include "core/graphics.h"
#include "core/text.h"
#include "core/version.h"
#include "core/storage.h"
#include "state.h"
#include "ui/ui.h"
#include "wifi_screen.h"

namespace settingsScreen {

namespace {

constexpr const char* SETTINGS_FILE = "settings";

// List rows as the home page draws them: the setting on the left, its value on the right.
constexpr int ROW_WIDTH = SCREEN_WIDTH - 2 * ui::MARGIN;
constexpr int ROW_HEIGHT = ui::ROW_HEIGHT;
constexpr int ROW_STEP = ROW_HEIGHT + ui::ROW_GAP;
constexpr int WIFI_ROW_TOP = ui::CONTENT_TOP;
constexpr int SLEEP_ROW_TOP = WIFI_ROW_TOP + ROW_STEP;
constexpr int ZONE_ROW_TOP = SLEEP_ROW_TOP + ROW_STEP;
constexpr int REFRESH_ROW_TOP = ZONE_ROW_TOP + ROW_STEP;
constexpr int ABOUT_ROW_TOP = REFRESH_ROW_TOP + ROW_STEP;
constexpr int VALUE_INSET = 18;
constexpr int VALUE_RIGHT = ui::MARGIN + ROW_WIDTH - VALUE_INSET;
// The Wi-Fi value has the row less its label; "WI-FI" is under 80 px at Size20.
constexpr int WIFI_VALUE_WIDTH = ROW_WIDTH - 2 * VALUE_INSET - 80 - ui::GAP;

Settings settings;
bool failed = false;

void save() {
  if (!storage::ready()) {
    failed = false;
    return;
  }
  uint8_t data[SETTINGS_BYTES];
  const size_t size = packSettings(settings, data);
  failed = !storage::save(SETTINGS_FILE, SETTINGS_VERSION, data, size);
}

void cycleIdleSleep() {
  settings.idleSleepMinutes = settings.idleSleepMinutes == 1 ? 5 : settings.idleSleepMinutes == 5 ? 10 : 1;
  Serial.printf("Idle sleep is now %u minutes\n", settings.idleSleepMinutes);
  save();
}

void cycleRefresh() {
  settings.refreshChoice = (settings.refreshChoice + 1) % REFRESH_CHOICE_COUNT;
  Serial.printf("Clean-up refresh every %d updates (0 is never)\n", updatesPerCleanup());
  save();
}

}  // namespace

// A file that's missing or can't be trusted just leaves the defaults in place.
void begin() {
  if (!storage::ready()) return;

  uint8_t data[SETTINGS_BYTES];
  uint8_t version = 0;
  size_t size = 0;
  if (storage::load(SETTINGS_FILE, version, data, sizeof(data), size) && !unpackSettings(version, data, size, settings)) {
    Serial.println("Ignoring saved settings");
  }
  wallClock::setZone(settings.timeZone);
}

uint8_t idleSleepMinutes() { return settings.idleSleepMinutes; }

int updatesPerCleanup() { return REFRESH_CHOICES[settings.refreshChoice]; }

int timeZone() { return settings.timeZone; }

void setTimeZone(int index) {
  settings.timeZone = static_cast<uint8_t>(index);
  wallClock::setZone(index);
  Serial.printf("Time zone is now %s\n", wallClock::zone(index).name);
  save();
}

Tap onTap(int x, int y) {
  if (ui::isOnBack(x, y)) return Tap::Back;
  if (ui::isInBox(x, y, ui::MARGIN, WIFI_ROW_TOP, ROW_WIDTH, ROW_HEIGHT)) return Tap::OpenWifi;
  if (ui::isInBox(x, y, ui::MARGIN, SLEEP_ROW_TOP, ROW_WIDTH, ROW_HEIGHT)) {
    cycleIdleSleep();
    return Tap::Changed;
  }
  if (ui::isInBox(x, y, ui::MARGIN, ZONE_ROW_TOP, ROW_WIDTH, ROW_HEIGHT)) return Tap::OpenTimeZone;
  if (ui::isInBox(x, y, ui::MARGIN, REFRESH_ROW_TOP, ROW_WIDTH, ROW_HEIGHT)) {
    cycleRefresh();
    return Tap::Changed;
  }
  if (ui::isInBox(x, y, ui::MARGIN, ABOUT_ROW_TOP, ROW_WIDTH, ROW_HEIGHT)) return Tap::OpenAbout;
  return Tap::Nothing;
}

namespace {

void drawSetting(uint8_t* frame, int top, const char* label, const char* value) {
  ui::drawRow(frame, top, label, nullptr);
  const int cap = capHeight(Font::Size20);
  drawText(frame, Font::Size20, VALUE_RIGHT, top + (ROW_HEIGHT - cap) / 2 + cap, value, Align::Right);
}

}  // namespace

void draw(uint8_t* frame) {
  ui::drawBand(frame, "SETTINGS", nullptr, true);

  char value[48];
  wifiScreen::statusValue(value, sizeof(value), WIFI_VALUE_WIDTH);
  drawSetting(frame, WIFI_ROW_TOP, "WI-FI", value);

  snprintf(value, sizeof(value), "%u MIN", settings.idleSleepMinutes);
  drawSetting(frame, SLEEP_ROW_TOP, "SLEEP AFTER", value);

  wallClock::formatOffset(settings.timeZone, value, sizeof(value));
  drawSetting(frame, ZONE_ROW_TOP, "TIME ZONE", value);

  if (updatesPerCleanup() == 0) {
    snprintf(value, sizeof(value), "NEVER");
  } else {
    snprintf(value, sizeof(value), "EVERY %d", updatesPerCleanup());
  }
  drawSetting(frame, REFRESH_ROW_TOP, "REFRESH", value);

  drawSetting(frame, ABOUT_ROW_TOP, "ABOUT", version::NUMBER);
}

bool saveFailed() { return failed; }

void retrySave() { save(); }

}  // namespace settingsScreen
