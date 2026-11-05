#include "clock.h"

#include <Arduino.h>
#include <Rtc.h>
#include <sys/time.h>

#include <cstdio>
#include <cstdlib>
#include <ctime>

namespace wallClock {

namespace {

// CrossPlay's list, from src/util/Timezones.cpp. West to east, with fixed offsets
// after the named zones. Settings saves a position in this list, so only ever add
// to the end.
constexpr Zone ZONES[] = {
    {"Midway", "SST11", -44},
    {"Honolulu", "HST10", -40},
    {"Anchorage", "AKST9AKDT,M3.2.0,M11.1.0", -36},
    {"Los Angeles / Vancouver", "PST8PDT,M3.2.0,M11.1.0", -32},
    {"Denver / Edmonton", "MST7MDT,M3.2.0,M11.1.0", -28},
    {"Phoenix", "MST7", -28},
    {"Chicago / Winnipeg", "CST6CDT,M3.2.0,M11.1.0", -24},
    {"Mexico City", "CST6", -24},
    {"New York / Toronto", "EST5EDT,M3.2.0,M11.1.0", -20},
    {"Bogota / Lima", "COT5", -20},
    {"Halifax", "AST4ADT,M3.2.0,M11.1.0", -16},
    {"Caracas / La Paz", "VET4", -16},
    {"Santiago", "CLT4CLST,M9.1.6/24,M4.1.6/24", -16},
    {"St. John's", "NST3:30NDT,M3.2.0,M11.1.0", -14},
    {"Buenos Aires / Montevideo", "ART3", -12},
    {"Sao Paulo", "BRT3", -12},
    {"Azores", "AZOT1AZOST,M3.5.0/0,M10.5.0/1", -4},
    {"UTC", "UTC0", 0},
    {"London / Dublin / Lisbon", "GMT0BST,M3.5.0/1,M10.5.0", 0},
    {"Berlin / Paris / Madrid / Rome", "CET-1CEST,M3.5.0,M10.5.0/3", 4},
    {"Lagos / Algiers", "WAT-1", 4},
    {"Athens / Helsinki / Kyiv", "EET-2EEST,M3.5.0/3,M10.5.0/4", 8},
    {"Cairo", "EET-2EEST,M4.5.5/0,M10.5.4/24", 8},
    {"Jerusalem", "IST-2IDT,M3.4.4/26,M10.5.0", 8},
    {"Johannesburg", "SAST-2", 8},
    {"Moscow / Istanbul / Riyadh", "MSK-3", 12},
    {"Nairobi", "EAT-3", 12},
    {"Tehran", "IRST-3:30", 14},
    {"Dubai / Tbilisi", "GST-4", 16},
    {"Kabul", "AFT-4:30", 18},
    {"Karachi / Tashkent", "PKT-5", 20},
    {"India / Colombo", "IST-5:30", 22},
    {"Kathmandu", "NPT-5:45", 23},
    {"Dhaka / Almaty", "BST-6", 24},
    {"Yangon", "MMT-6:30", 26},
    {"Bangkok / Jakarta / Hanoi", "ICT-7", 28},
    {"China / Hong Kong / Taipei", "CST-8", 32},
    {"Singapore / Manila / Kuala Lumpur", "SGT-8", 32},
    {"Perth", "AWST-8", 32},
    {"Tokyo / Seoul", "JST-9", 36},
    {"Darwin", "ACST-9:30", 38},
    {"Adelaide", "ACST-9:30ACDT,M10.1.0,M4.1.0/3", 38},
    {"Brisbane / Guam", "AEST-10", 40},
    {"Sydney / Melbourne", "AEST-10AEDT,M10.1.0,M4.1.0/3", 40},
    {"Honiara / Noumea", "SBT-11", 44},
    {"Auckland", "NZST-12NZDT,M9.5.0,M4.1.0/3", 48},
    {"Fiji", "FJT-12", 48},
    {"Nuku'alofa", "TOT-13", 52},
    {"Kiritimati", "LINT-14", 56},
    {"UTC-12:00", "UTC12", -48},
    {"UTC-11:30", "UTC11:30", -46},
    {"UTC-11:00", "UTC11", -44},
    {"UTC-10:30", "UTC10:30", -42},
    {"UTC-10:00", "UTC10", -40},
    {"UTC-09:30", "UTC9:30", -38},
    {"UTC-09:00", "UTC9", -36},
    {"UTC-08:30", "UTC8:30", -34},
    {"UTC-08:00", "UTC8", -32},
    {"UTC-07:30", "UTC7:30", -30},
    {"UTC-07:00", "UTC7", -28},
    {"UTC-06:30", "UTC6:30", -26},
    {"UTC-06:00", "UTC6", -24},
    {"UTC-05:30", "UTC5:30", -22},
    {"UTC-05:00", "UTC5", -20},
    {"UTC-04:30", "UTC4:30", -18},
    {"UTC-04:00", "UTC4", -16},
    {"UTC-03:30", "UTC3:30", -14},
    {"UTC-03:00", "UTC3", -12},
    {"UTC-02:30", "UTC2:30", -10},
    {"UTC-02:00", "UTC2", -8},
    {"UTC-01:30", "UTC1:30", -6},
    {"UTC-01:00", "UTC1", -4},
    {"UTC-00:30", "UTC0:30", -2},
    {"UTC+00:30", "UTC-0:30", 2},
    {"UTC+01:00", "UTC-1", 4},
    {"UTC+01:30", "UTC-1:30", 6},
    {"UTC+02:00", "UTC-2", 8},
    {"UTC+02:30", "UTC-2:30", 10},
    {"UTC+03:00", "UTC-3", 12},
    {"UTC+03:30", "UTC-3:30", 14},
    {"UTC+04:00", "UTC-4", 16},
    {"UTC+04:30", "UTC-4:30", 18},
    {"UTC+05:00", "UTC-5", 20},
    {"UTC+05:30", "UTC-5:30", 22},
    {"UTC+05:45", "UTC-5:45", 23},
    {"UTC+06:00", "UTC-6", 24},
    {"UTC+06:30", "UTC-6:30", 26},
    {"UTC+07:00", "UTC-7", 28},
    {"UTC+07:30", "UTC-7:30", 30},
    {"UTC+08:00", "UTC-8", 32},
    {"UTC+08:30", "UTC-8:30", 34},
    {"UTC+08:45", "UTC-8:45", 35},
    {"UTC+09:00", "UTC-9", 36},
    {"UTC+09:30", "UTC-9:30", 38},
    {"UTC+10:00", "UTC-10", 40},
    {"UTC+10:30", "UTC-10:30", 42},
    {"UTC+11:00", "UTC-11", 44},
    {"UTC+11:30", "UTC-11:30", 46},
    {"UTC+12:00", "UTC-12", 48},
    {"UTC+12:30", "UTC-12:30", 50},
    {"UTC+12:45", "UTC-12:45", 51},
    {"UTC+13:00", "UTC-13", 52},
    {"UTC+13:30", "UTC-13:30", 54},
    {"UTC+14:00", "UTC-14", 56},
};

static_assert(sizeof(ZONES) / sizeof(ZONES[0]) == ZONE_COUNT, "ZONE_COUNT must match the list");
static_assert(ZONES[UTC_ZONE].stdOffsetQuarters == 0, "UTC_ZONE must point at UTC");

// The RTC keeps UTC. It's on the sensor bus, which has its own I2C controller, so
// it doesn't share a bus with touch.
Rtc rtc;
int currentZone = UTC_ZONE;

// Howard Hinnant's days_from_civil. Using this rather than mktime() keeps the RTC's
// UTC value independent of whichever zone is set.
long daysFromCivil(int year, int month, int day) {
  year -= month <= 2 ? 1 : 0;
  const long era = (year >= 0 ? year : year - 399) / 400;
  const long yearOfEra = year - era * 400;
  const long dayOfYear = (153L * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const long dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
  return era * 146097 + dayOfEra - 719468;
}

}  // namespace

const Zone& zone(int index) { return ZONES[index >= 0 && index < ZONE_COUNT ? index : UTC_ZONE]; }

void formatOffset(int index, char* out, size_t size) {
  const int quarters = zone(index).stdOffsetQuarters;
  if (quarters == 0) {
    snprintf(out, size, "UTC");
    return;
  }
  const int magnitude = quarters < 0 ? -quarters : quarters;
  snprintf(out, size, "UTC%c%d:%02d", quarters < 0 ? '-' : '+', magnitude / 4, (magnitude % 4) * 15);
}

void begin() {
  if (!rtc.begin()) {
    Serial.println("No clock chip found");
    return;
  }

  // A year before 2020 means the chip was never set.
  Rtc::DateTime now;
  if (!rtc.now(now) || now.year < 2020) {
    Serial.println("The clock chip hasn't been set");
    return;
  }

  const long days = daysFromCivil(now.year, now.month, now.day);
  timeval value = {};
  value.tv_sec = static_cast<time_t>(days) * 86400 + now.hour * 3600L + now.minute * 60L + now.second;
  settimeofday(&value, nullptr);
  Serial.printf("Clock %04u-%02u-%02u %02u:%02u UTC\n", now.year, now.month, now.day, now.hour, now.minute);
}

void setZone(int index) {
  currentZone = index >= 0 && index < ZONE_COUNT ? index : UTC_ZONE;
  setenv("TZ", zone(currentZone).posixTz, 1);
  tzset();
}

const char* posixTz() { return zone(currentZone).posixTz; }

bool isSet() {
  const time_t now = time(nullptr);
  tm utc;
  gmtime_r(&now, &utc);
  return utc.tm_year + 1900 >= 2021;
}

void saveToChip() {
  if (!rtc.present()) return;

  const time_t now = time(nullptr);
  tm utc;
  gmtime_r(&now, &utc);

  Rtc::DateTime value;
  value.year = static_cast<uint16_t>(utc.tm_year + 1900);
  value.month = static_cast<uint8_t>(utc.tm_mon + 1);
  value.day = static_cast<uint8_t>(utc.tm_mday);
  value.hour = static_cast<uint8_t>(utc.tm_hour);
  value.minute = static_cast<uint8_t>(utc.tm_min);
  value.second = static_cast<uint8_t>(utc.tm_sec);
  value.weekday = static_cast<uint8_t>(utc.tm_wday);
  if (!rtc.set(value)) Serial.println("ERROR: could not set the clock chip");
}

}  // namespace wallClock
