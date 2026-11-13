#pragma once

// The battery, read from the BQ27220 fuel gauge. Call these from loop() only: the
// gauge shares its I2C bus with the clock chip, and nothing guards that bus.
namespace battery {

struct Reading {
  int percent;    // 0..100, or -1 if the gauge has never answered
  bool charging;  // current is flowing into the battery, so false once it's full
};

// Reads the gauge for drawing, and remembers the result as what's on the panel.
Reading read();

// Call every loop while the battery is on screen. Checks the gauge every few seconds
// and returns true when the screen should redraw.
bool poll();

}  // namespace battery
