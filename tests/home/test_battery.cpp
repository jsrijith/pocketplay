// The battery icon's fill and when the battery redraws, checked on the PC.
#include <cstdio>
#include <cstdlib>

#include "core/battery_state.h"
#include "ui/battery_icon.h"

namespace {

int failures = 0;

void expect(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    failures++;
  }
}

void testFill() {
  using ui::batteryFillWidth;

  expect(batteryFillWidth(0, 22) == 0, "an empty battery fills nothing");
  expect(batteryFillWidth(100, 22) == 22, "a full battery fills everything");
  expect(batteryFillWidth(50, 22) == 11, "half fills half");
  expect(batteryFillWidth(1, 22) == 1, "any charge at all shows");
  expect(batteryFillWidth(99, 22) == 22, "rounds up");
  expect(batteryFillWidth(-1, 22) == 0, "unknown fills nothing");
  expect(batteryFillWidth(150, 22) == 22, "above 100 stays inside the icon");
}

void testChargingChange() {
  battery::RedrawCheck check;
  check.shown(80, false);
  expect(!check.check(80, false), "nothing changed, no redraw");
  expect(!check.check(80, true), "the first check of a change waits");
  expect(check.check(80, true), "the second check of a change redraws");

  check.shown(80, true);
  expect(!check.check(80, false), "unplugging waits one check too");
  expect(check.check(80, false), "unplugging redraws");
}

void testBlip() {
  battery::RedrawCheck check;
  check.shown(99, true);
  expect(!check.check(99, false), "a one-check blip waits");
  expect(!check.check(99, true), "and goes away without a redraw");
  expect(!check.check(99, false), "a new blip starts counting again");
  expect(!check.check(99, true), "so swinging current never redraws");
}

void testPercent() {
  battery::RedrawCheck check;
  check.shown(80, true);
  expect(!check.check(81, true), "a climbing percent waits one check");
  expect(check.check(81, true), "then redraws while charging");

  check.shown(80, false);
  for (int percent = 79; percent > 70; percent--) {
    expect(!check.check(percent, false), "a falling percent on battery never redraws");
  }
}

void testRedrawResets() {
  battery::RedrawCheck check;
  check.shown(80, false);
  check.check(80, true);
  check.check(80, true);
  check.shown(80, true);
  expect(!check.check(80, true), "once drawn, the same state asks for nothing");
  expect(!check.check(80, false), "and a later change starts counting from zero");
}

void testUnknown() {
  battery::RedrawCheck check;
  check.shown(-1, false);
  expect(!check.check(-1, false), "still unknown, no redraw");
  expect(!check.check(70, false), "a first good reading waits one check");
  expect(check.check(70, false), "then replaces the unknown mark, even on battery");

  check.shown(-1, true);
  expect(!check.check(-1, true), "unknown is not a climbing percent");
}

}  // namespace

int main() {
  testFill();
  testChargingChange();
  testBlip();
  testPercent();
  testRedrawResets();
  testUnknown();

  if (failures != 0) return EXIT_FAILURE;
  std::printf("battery: all passed\n");
  return EXIT_SUCCESS;
}
