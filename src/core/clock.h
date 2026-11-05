#pragma once

#include <cstddef>
#include <cstdint>

// The date and time: the clock chip, and which time zone to read them in. The system
// clock and the chip both keep UTC, and localtime_r() applies the zone. A clock chip
// keeps time with the power off, which the ESP32's own clock can't.
namespace wallClock {

constexpr int ZONE_COUNT = 104;
constexpr uint8_t UTC_ZONE = 17;

struct Zone {
  const char* name;
  const char* posixTz;        // the rule localtime_r() follows, including daylight saving
  int16_t stdOffsetQuarters;  // standard offset from UTC, in quarter hours
};

const Zone& zone(int index);

// Writes "UTC+5:30", or "UTC" for no offset.
void formatOffset(int index, char* out, size_t size);

// Sets the system clock from the chip. Call once at boot.
void begin();

void setZone(int index);
const char* posixTz();

// False until the chip or a time server has given a real date.
bool isSet();

// Call when a time server has just set the system clock.
void saveToChip();

}  // namespace wallClock
