#include "battery.h"

#include <Arduino.h>
#include <BatteryMonitor.h>

#include <cstdint>

#include "battery_state.h"

namespace battery {

namespace {

constexpr uint32_t POLL_MS = 5000;

RedrawCheck redrawCheck;
uint32_t lastPollMs = 0;
int lastPercent = -1;

Reading sample() {
  // Made on first use, because its constructor sets GPIO40's pin mode, which has to
  // wait until setup() has started.
  static const BatteryMonitor monitor;

  // A failed read keeps the last good value, so a bus error can't show a false 0%.
  uint16_t percent = 0;
  if (monitor.readPercentageChecked(percent)) lastPercent = percent;
  return {lastPercent, monitor.isCharging()};
}

}  // namespace

Reading read() {
  const Reading reading = sample();
  redrawCheck.shown(reading.percent, reading.charging);
  lastPollMs = millis();
  return reading;
}

bool poll() {
  const uint32_t now = millis();
  if (now - lastPollMs < POLL_MS) return false;
  lastPollMs = now;

  const Reading reading = sample();
  return redrawCheck.check(reading.percent, reading.charging);
}

}  // namespace battery
