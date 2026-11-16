#pragma once

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// A pocket calculator's rules, with no screen or card code, so tests/calculator/ can
// check them on the PC. Pocket style: each operator works out the sum so far straight
// away, so 2 + 3 x 4 = 20, as on a desk calculator.
namespace calculator {

enum class Op : unsigned char { None, Add, Subtract, Multiply, Divide };
constexpr int OP_COUNT = 5;

enum class Key {
  Digit0, Digit1, Digit2, Digit3, Digit4, Digit5, Digit6, Digit7, Digit8, Digit9,
  Point, Add, Subtract, Multiply, Divide, Equals, Percent, Clear, Backspace,
};

constexpr int MAX_DIGITS = 12;
// A result this big or bigger is an error, as it can't be shown in 12 digits.
constexpr double LIMIT = 1e12;
// A sign, the digits and a point.
constexpr int MAX_ENTRY = MAX_DIGITS + 2;

struct State {
  // What the display shows: the number being typed, or the last result. Always a
  // valid number: an optional '-', up to 12 digits and at most one '.'.
  char entry[MAX_ENTRY + 1] = "0";
  double accumulator = 0;  // the number before the pending operator
  Op pending = Op::None;
  // The last sum's operator and second number, so pressing = again repeats it.
  Op lastOp = Op::None;
  double lastOperand = 0;
  bool fresh = true;  // the next digit starts a new number
  bool error = false;
};

inline bool sameState(const State& a, const State& b) {
  return strcmp(a.entry, b.entry) == 0 && a.accumulator == b.accumulator && a.pending == b.pending &&
         a.lastOp == b.lastOp && a.lastOperand == b.lastOperand && a.fresh == b.fresh && a.error == b.error;
}

inline int digitCount(const char* text) {
  int count = 0;
  for (const char* c = text; *c != '\0'; c++) {
    if (*c >= '0' && *c <= '9') count++;
  }
  return count;
}

// A typed entry or a shown result: an optional leading '-', at least one digit,
// at most 12 digits and at most one '.'. Anything else is rejected.
inline bool validEntry(const char* text) {
  const size_t length = strlen(text);
  if (length == 0 || length > static_cast<size_t>(MAX_ENTRY)) return false;
  int points = 0;
  for (size_t i = 0; i < length; i++) {
    const char c = text[i];
    if (c == '-' && i == 0) continue;
    if (c == '.') {
      points++;
      continue;
    }
    if (c < '0' || c > '9') return false;
  }
  const int digits = digitCount(text);
  return points <= 1 && digits >= 1 && digits <= MAX_DIGITS;
}

// Writes value with at most 12 significant digits and no trailing zeros, so
// 0.1 + 0.2 shows 0.3. Returns false if it doesn't fit in 12 digits.
inline bool format(double value, char* out, size_t outSize) {
  if (!std::isfinite(value) || std::fabs(value) >= LIMIT) return false;
  char whole[32];
  std::snprintf(whole, sizeof(whole), "%.0f", std::fabs(value));
  const int wholeDigits = static_cast<int>(strlen(whole));
  if (wholeDigits > MAX_DIGITS) return false;
  const int decimals = MAX_DIGITS - wholeDigits;
  char text[40];
  std::snprintf(text, sizeof(text), "%.*f", decimals, value);
  if (strchr(text, '.') != nullptr) {
    size_t end = strlen(text);
    while (end > 0 && text[end - 1] == '0') text[--end] = '\0';
    if (end > 0 && text[end - 1] == '.') text[--end] = '\0';
  }
  // A tiny negative rounds to "-0", which is just 0.
  if (strcmp(text, "-0") == 0) std::snprintf(text, sizeof(text), "0");
  if (digitCount(text) > MAX_DIGITS || strlen(text) >= outSize) return false;
  std::snprintf(out, outSize, "%s", text);
  return true;
}

inline double valueOf(const State& s) { return std::strtod(s.entry, nullptr); }

inline void fail(State& s) {
  s = State{};
  s.error = true;
}

// Shows a result, or the error if it can't be shown. The next digit starts afresh.
inline void show(State& s, double value) {
  if (!format(value, s.entry, sizeof(s.entry))) {
    fail(s);
    return;
  }
  s.fresh = true;
}

// Returns false for a division by zero, which has no answer.
inline bool apply(double a, Op op, double b, double& result) {
  switch (op) {
    case Op::None:
      result = b;
      return true;
    case Op::Add:
      result = a + b;
      return true;
    case Op::Subtract:
      result = a - b;
      return true;
    case Op::Multiply:
      result = a * b;
      return true;
    case Op::Divide:
      if (b == 0) return false;
      result = a / b;
      return true;
  }
  return false;
}

inline void workOut(State& s, double a, Op op, double b) {
  double result = 0;
  if (!apply(a, op, b, result)) {
    fail(s);
    return;
  }
  show(s, result);
}

inline void typeDigit(State& s, char digit) {
  if (s.fresh) {
    s.entry[0] = digit;
    s.entry[1] = '\0';
    s.fresh = false;
    return;
  }
  if (strcmp(s.entry, "0") == 0) {
    s.entry[0] = digit;
    return;
  }
  if (digitCount(s.entry) >= MAX_DIGITS) return;
  const size_t length = strlen(s.entry);
  s.entry[length] = digit;
  s.entry[length + 1] = '\0';
}

inline void typePoint(State& s) {
  if (s.fresh) {
    std::snprintf(s.entry, sizeof(s.entry), "0.");
    s.fresh = false;
    return;
  }
  if (strchr(s.entry, '.') != nullptr || digitCount(s.entry) >= MAX_DIGITS) return;
  const size_t length = strlen(s.entry);
  s.entry[length] = '.';
  s.entry[length + 1] = '\0';
}

// Only a number being typed can lose a digit; a result can't.
inline void backspace(State& s) {
  if (s.fresh) return;
  size_t length = strlen(s.entry);
  s.entry[--length] = '\0';
  if (length == 0 || strcmp(s.entry, "-") == 0) std::snprintf(s.entry, sizeof(s.entry), "0");
}

inline void chooseOp(State& s, Op op) {
  // A second operator straight after the first only replaces it.
  const bool replacing = s.pending != Op::None && s.fresh;
  if (!replacing) {
    if (s.pending != Op::None) {
      // The sum so far, worked out at once: pocket style.
      workOut(s, s.accumulator, s.pending, valueOf(s));
      if (s.error) return;
    }
    s.accumulator = valueOf(s);
  }
  s.pending = op;
  s.lastOp = Op::None;
  s.fresh = true;
}

// With a sum waiting, = works it out, using the shown number as the second one, so
// 5 + = gives 10. With none, = repeats the last sum on the result: 2 + 3 = = gives 8.
inline void equals(State& s) {
  if (s.pending != Op::None) {
    const Op op = s.pending;
    const double operand = valueOf(s);
    s.pending = Op::None;
    workOut(s, s.accumulator, op, operand);
    if (s.error) return;
    s.lastOp = op;
    s.lastOperand = operand;
  } else if (s.lastOp != Op::None) {
    const Op op = s.lastOp;
    const double operand = s.lastOperand;
    workOut(s, valueOf(s), op, operand);
    if (s.error) return;
    s.lastOp = op;
    s.lastOperand = operand;
  } else {
    s.fresh = true;
  }
}

// Turns the shown number into a percentage, ready for =: with + or - waiting it's a
// percent of the first number, so 200 + 10 % shows 20 and = gives 220. With x or /
// waiting, or nothing, it's the number over 100.
inline void percent(State& s) {
  const double b = valueOf(s);
  const bool ofFirst = s.pending == Op::Add || s.pending == Op::Subtract;
  show(s, ofFirst ? s.accumulator * b / 100 : b / 100);
}

// Returns true if the press changed anything, so a press that changes nothing can
// leave the screen alone.
inline bool press(State& s, Key key) {
  const State before = s;
  if (key == Key::Clear) {
    s = State{};
    return !sameState(before, s);
  }
  if (s.error) {
    // Only C, or a new number, gets out of an error.
    const bool digit = key <= Key::Digit9 || key == Key::Point;
    if (!digit) return false;
    s = State{};
  }
  switch (key) {
    case Key::Point:
      typePoint(s);
      break;
    case Key::Add:
      chooseOp(s, Op::Add);
      break;
    case Key::Subtract:
      chooseOp(s, Op::Subtract);
      break;
    case Key::Multiply:
      chooseOp(s, Op::Multiply);
      break;
    case Key::Divide:
      chooseOp(s, Op::Divide);
      break;
    case Key::Equals:
      equals(s);
      break;
    case Key::Percent:
      percent(s);
      break;
    case Key::Backspace:
      backspace(s);
      break;
    case Key::Clear:
      break;
    default:
      typeDigit(s, static_cast<char>('0' + static_cast<int>(key)));
      break;
  }
  return !sameState(before, s);
}

}  // namespace calculator
