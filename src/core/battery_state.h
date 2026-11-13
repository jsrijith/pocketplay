#pragma once

// Decides when the battery on screen is out of date enough to redraw, kept free of
// hardware so tests/home/ can check it on the PC.
namespace battery {

class RedrawCheck {
 public:
  // What's now on the panel. A percent below 0 means no good reading yet.
  void shown(int percent, bool charging) {
    shownPercent = percent;
    shownCharging = charging;
    differentChecks = 0;
  }

  // One check of the gauge. True if the screen should redraw.
  //
  // A change must show on two checks in a row, because the current can hover around
  // zero as the battery nears full, and a blink for each swing would look like a bug.
  // A falling percent on battery never redraws: e-ink holds its image for free, and
  // the number is read afresh whenever the page is next drawn.
  bool check(int percent, bool charging) {
    const bool chargingChanged = charging != shownCharging;
    const bool percentClimbing = charging && percent >= 0 && percent != shownPercent;
    const bool firstReading = shownPercent < 0 && percent >= 0;
    if (!chargingChanged && !percentClimbing && !firstReading) {
      differentChecks = 0;
      return false;
    }
    if (differentChecks < CHECKS_TO_REDRAW) differentChecks++;
    return differentChecks >= CHECKS_TO_REDRAW;
  }

 private:
  static constexpr int CHECKS_TO_REDRAW = 2;

  int shownPercent = -1;
  bool shownCharging = false;
  int differentChecks = 0;
};

}  // namespace battery
