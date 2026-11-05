#include <cstdio>

#include "settings/state.h"

static int failures = 0;

#define CHECK(cond)                                  \
  do {                                               \
    if (!(cond)) {                                   \
      ++failures;                                    \
      std::printf("FAIL line %d: %s\n", __LINE__, #cond); \
    }                                                \
  } while (0)

int main() {
  // Files from older firmware keep what they have and default the rest.
  {
    Settings settings;
    const uint8_t version1[] = {5};
    CHECK(unpackSettings(1, version1, sizeof(version1), settings));
    CHECK(settings.idleSleepMinutes == 5);
    CHECK(settings.timeZone == wallClock::UTC_ZONE);
    CHECK(REFRESH_CHOICES[settings.refreshChoice] == 30);
  }
  {
    Settings settings;
    const uint8_t version2[] = {1, 31};
    CHECK(unpackSettings(2, version2, sizeof(version2), settings));
    CHECK(settings.idleSleepMinutes == 1);
    CHECK(settings.timeZone == 31);
    CHECK(REFRESH_CHOICES[settings.refreshChoice] == 30);
  }

  // The current layout goes both ways, including "never".
  {
    Settings saved;
    saved.idleSleepMinutes = 1;
    saved.timeZone = 31;
    saved.refreshChoice = REFRESH_CHOICE_COUNT - 1;
    uint8_t data[SETTINGS_BYTES];
    const size_t size = packSettings(saved, data);
    Settings loaded;
    CHECK(unpackSettings(SETTINGS_VERSION, data, size, loaded));
    CHECK(loaded.idleSleepMinutes == 1);
    CHECK(loaded.timeZone == 31);
    CHECK(REFRESH_CHOICES[loaded.refreshChoice] == 0);
  }

  // Anything else changes nothing.
  {
    Settings settings;
    settings.idleSleepMinutes = 5;
    const uint8_t badZone[] = {1, wallClock::ZONE_COUNT, 0};
    const uint8_t badSleep[] = {7, 0, 0};
    const uint8_t badRefresh[] = {1, 0, REFRESH_CHOICE_COUNT};
    const uint8_t good[] = {1, 0, 0, 0};
    CHECK(!unpackSettings(SETTINGS_VERSION, badZone, sizeof(badZone), settings));
    CHECK(!unpackSettings(SETTINGS_VERSION, badSleep, sizeof(badSleep), settings));
    CHECK(!unpackSettings(SETTINGS_VERSION, badRefresh, sizeof(badRefresh), settings));
    CHECK(!unpackSettings(SETTINGS_VERSION + 1, good, sizeof(good), settings));
    CHECK(!unpackSettings(SETTINGS_VERSION, good, 2, settings));
    CHECK(!unpackSettings(1, good, 2, settings));
    CHECK(!unpackSettings(0, good, 0, settings));
    CHECK(settings.idleSleepMinutes == 5);
    CHECK(settings.timeZone == wallClock::UTC_ZONE);
  }

  std::printf(failures == 0 ? "settings: all passed\n" : "settings: %d failed\n", failures);
  return failures == 0 ? 0 : 1;
}
