// Calculator with no panel: typing and its limits, pocket-style chaining, repeated =,
// percent, errors and how to leave them, backspace, how results are written, presses
// that change nothing, and the save.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

#include "calculator_rules.h"
#include "calculator_save.h"

using namespace calculator;

namespace {

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    failures++;
  }
}

Key keyFor(char c) {
  switch (c) {
    case '.':
      return Key::Point;
    case '+':
      return Key::Add;
    case '-':
      return Key::Subtract;
    case '*':
      return Key::Multiply;
    case '/':
      return Key::Divide;
    case '=':
      return Key::Equals;
    case '%':
      return Key::Percent;
    case 'C':
      return Key::Clear;
    case '<':
      return Key::Backspace;
    default:
      return static_cast<Key>(c - '0');
  }
}

// Presses each key in turn, from a fresh calculator, and returns what's shown.
State run(const char* keys) {
  State s;
  for (const char* k = keys; *k != '\0'; k++) press(s, keyFor(*k));
  return s;
}

bool shows(const char* keys, const char* expected) {
  const State s = run(keys);
  if (s.error || strcmp(s.entry, expected) != 0) {
    std::printf("  %s shows %s%s, expected %s\n", keys, s.entry, s.error ? " (error)" : "", expected);
    return false;
  }
  return true;
}

bool pressChanges(const char* setup, char key) {
  State s = run(setup);
  return press(s, keyFor(key));
}

void testTyping() {
  check(shows("", "0"), "it starts at 0");
  check(shows("123", "123"), "digits are typed in order");
  check(shows("007", "7"), "leading zeros are dropped");
  check(shows("1.5", "1.5"), "a point is typed");
  check(shows(".5", "0.5"), "a point first gets a 0 before it");
  check(shows("1.2.3", "1.23"), "a second point is ignored");
  check(shows("1234567890123", "123456789012"), "a 13th digit is ignored");
  check(shows("0.00000000001", "0.00000000001"), "12 digits counting the leading 0");
  check(!pressChanges("123456789012", '3'), "a 13th digit changes nothing");
  check(!pressChanges("1.5", '.'), "a second point changes nothing");
  check(!pressChanges("0", '0'), "0 on 0 changes nothing once typing");
}

void testChaining() {
  check(shows("2+3=", "5"), "2 + 3 = 5");
  check(shows("2+3*4=", "20"), "pocket style: 2 + 3 x 4 = 20");
  check(shows("2+3*", "5"), "an operator shows the sum so far");
  check(shows("9-12=", "-3"), "a negative result");
  check(shows("7/2=", "3.5"), "division");
  check(shows("2+*3=", "6"), "a second operator replaces the first");
  check(!pressChanges("2+", '+'), "the same operator again changes nothing");
  check(shows("5+=", "10"), "= with no second number uses the shown one");
  check(shows("2+3==", "8"), "= again repeats the last sum");
  check(shows("2*3===", "54"), "and keeps repeating it");
  check(shows("2+3=4=", "7"), "a new number then = repeats the sum on it");
  check(shows("2+3=*2=", "10"), "a result carries into the next sum");
  check(shows("2+3=7", "7"), "a digit after = starts a new number");
  check(shows("1+2=C", "0"), "C clears");
  check(run("1+2C").pending == Op::None, "C clears the waiting operator");
}

void testPercent() {
  check(shows("200+10%", "20"), "with + waiting, % is a percent of the first number");
  check(shows("200+10%=", "220"), "200 + 10 % = 220");
  check(shows("200-10%=", "180"), "200 - 10 % = 180");
  check(shows("200*10%=", "20"), "200 x 10 % = 20");
  check(shows("200/10%=", "2000"), "200 / 10 % = 2000");
  check(shows("50%", "0.5"), "with nothing waiting, % is over 100");
}

void testErrors() {
  State s = run("5/0=");
  check(s.error, "dividing by zero is an error");
  check(run("5/0+").error, "an operator after dividing by zero works it out, so fails too");
  s = run("999999999999+1=");
  check(s.error, "a 13-digit result is an error");
  s = run("9999999*9999999=");
  check(s.error, "an overflowing product is an error");
  check(!pressChanges("5/0=", '+'), "an operator does nothing in an error");
  check(!pressChanges("5/0=", '='), "= does nothing in an error");
  check(!pressChanges("5/0=", '%'), "% does nothing in an error");
  check(!pressChanges("5/0=", '<'), "backspace does nothing in an error");
  check(shows("5/0=7", "7"), "a digit starts again after an error");
  check(shows("5/0=.", "0."), "a point starts again after an error");
  s = run("5/0=C");
  check(!s.error && strcmp(s.entry, "0") == 0, "C clears an error");
}

void testBackspace() {
  check(shows("123<", "12"), "backspace drops the last digit");
  check(shows("1<", "0"), "down to 0");
  check(shows("1.5<<", "1"), "and the point");
  check(!pressChanges("", '<'), "backspace on 0 changes nothing");
  check(!pressChanges("2+3=", '<'), "backspace on a result changes nothing");
  check(shows("9-12=*1=", "-3"), "a negative result carries on");
  check(shows("0-5=", "-5"), "a negative entry");
}

void testFormat() {
  char out[MAX_ENTRY + 1];
  check(format(0.1 + 0.2, out, sizeof(out)) && strcmp(out, "0.3") == 0, "0.1 + 0.2 shows 0.3");
  check(format(2.50, out, sizeof(out)) && strcmp(out, "2.5") == 0, "trailing zeros go");
  check(format(10, out, sizeof(out)) && strcmp(out, "10") == 0, "a whole number has no point");
  check(format(-0.5, out, sizeof(out)) && strcmp(out, "-0.5") == 0, "a negative");
  check(format(1.0 / 3, out, sizeof(out)) && strcmp(out, "0.33333333333") == 0, "a third, in 12 digits");
  check(format(1e-15, out, sizeof(out)) && strcmp(out, "0") == 0, "a tiny number is 0");
  check(format(-1e-15, out, sizeof(out)) && strcmp(out, "0") == 0, "a tiny negative is 0, not -0");
  check(format(999999999999, out, sizeof(out)) && strcmp(out, "999999999999") == 0, "the largest number");
  check(!format(1e12, out, sizeof(out)), "10^12 doesn't fit");
  check(!format(999999999999.6, out, sizeof(out)), "nor does a number that rounds up to it");
  check(!format(std::numeric_limits<double>::quiet_NaN(), out, sizeof(out)), "NaN doesn't format");
  check(shows("1/3=", "0.33333333333"), "a third on screen");
  check(shows("0.1+0.2=", "0.3"), "0.1 + 0.2 on screen");
}

void testSave() {
  const State before = run("12+3.5*");
  uint8_t data[SAVE_BYTES];
  const size_t size = pack(before, data);
  check(size == SAVE_BYTES, "pack writes every byte");
  State after;
  check(unpack(SAVE_VERSION, data, size, after) && sameState(before, after), "a round trip keeps everything");

  State error = run("5/0=");
  pack(error, data);
  check(unpack(SAVE_VERSION, data, size, after) && after.error, "an error survives a round trip");

  State untouched = run("42");
  State target = untouched;
  pack(run("7"), data);
  check(!unpack(SAVE_VERSION + 1, data, size, target) && sameState(target, untouched), "a wrong version is refused");
  check(!unpack(SAVE_VERSION, data, size - 1, target), "a short save is refused");

  pack(run("7"), data);
  data[1 + MAX_ENTRY + 16] = OP_COUNT;
  check(!unpack(SAVE_VERSION, data, size, target), "a bad operator is refused");

  pack(run("7"), data);
  data[1] = 'x';
  check(!unpack(SAVE_VERSION, data, size, target), "a bad character is refused");

  pack(run("7"), data);
  data[0] = MAX_ENTRY + 1;
  check(!unpack(SAVE_VERSION, data, size, target), "a long entry is refused");

  State nan = run("7");
  nan.accumulator = std::numeric_limits<double>::quiet_NaN();
  pack(nan, data);
  check(!unpack(SAVE_VERSION, data, size, target), "NaN is refused");

  pack(run("7"), data);
  data[SAVE_BYTES - 1] = 0x80;
  check(!unpack(SAVE_VERSION, data, size, target), "unknown flags are refused");
  check(sameState(target, untouched), "a refused save leaves the state alone");
}

}  // namespace

int main() {
  testTyping();
  testChaining();
  testPercent();
  testErrors();
  testBackspace();
  testFormat();
  testSave();
  if (failures != 0) {
    std::printf("%d calculator check(s) failed\n", failures);
    return 1;
  }
  std::printf("calculator: all checks passed\n");
  return 0;
}
