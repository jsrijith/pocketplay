#include "connections_save.h"

namespace connections {

namespace {

void put16(uint8_t*& out, uint32_t value) {
  *out++ = static_cast<uint8_t>(value);
  *out++ = static_cast<uint8_t>(value >> 8);
}

void put32(uint8_t*& out, uint32_t value) {
  put16(out, value & 0xFFFF);
  put16(out, value >> 16);
}

uint32_t get16(const uint8_t*& in) {
  const uint32_t value = static_cast<uint32_t>(in[0]) | (static_cast<uint32_t>(in[1]) << 8);
  in += 2;
  return value;
}

uint32_t get32(const uint8_t*& in) {
  const uint32_t low = get16(in);
  return low | (get16(in) << 16);
}

int bitCount(uint32_t value) {
  int count = 0;
  for (; value != 0; value &= value - 1) count++;
  return count;
}

// YYYYMMDD with a real month and day. The pack's own date is compared later; this
// only refuses bytes that can't be a date at all.
bool isDate(uint32_t date) {
  const uint32_t month = date / 100 % 100;
  const uint32_t day = date % 100;
  return date >= 20000101 && date <= 99991231 && month >= 1 && month <= 12 && day >= 1 && day <= 31;
}

}  // namespace

size_t packBoard(const OpenBoard& board, uint8_t* out) {
  uint8_t* at = out;
  put16(at, static_cast<uint32_t>(board.index));
  put32(at, board.date);
  put32(at, board.state.seed);
  *at++ = board.state.solvedCount;
  for (int i = 0; i < kGroups; i++) *at++ = board.state.solvedOrder[i];
  *at++ = board.state.mistakes;
  *at++ = board.state.guessCount;
  for (int i = 0; i < MAX_GUESSES; i++) put16(at, board.state.guessed[i]);
  put16(at, board.selected);
  return static_cast<size_t>(at - out);
}

bool unpackBoard(uint8_t version, const uint8_t* in, size_t size, OpenBoard& board) {
  if (version != SAVE_VERSION || size != SAVE_BYTES) return false;

  OpenBoard read;
  read.index = static_cast<int>(get16(in));
  read.date = get32(in);
  read.state.seed = get32(in);
  read.state.solvedCount = *in++;
  for (int i = 0; i < kGroups; i++) read.state.solvedOrder[i] = *in++;
  read.state.mistakes = *in++;
  read.state.guessCount = *in++;
  for (int i = 0; i < MAX_GUESSES; i++) read.state.guessed[i] = static_cast<uint16_t>(get16(in));
  read.selected = static_cast<uint16_t>(get16(in));

  if (!isDate(read.date)) return false;
  // A finished game is never saved, so all four solved or four mistakes is damage.
  if (read.state.solvedCount >= kGroups || read.state.mistakes >= kMaxMistakes) return false;
  for (int i = 0; i < read.state.solvedCount; i++) {
    if (read.state.solvedOrder[i] >= kGroups) return false;
    for (int j = 0; j < i; j++) {
      if (read.state.solvedOrder[i] == read.state.solvedOrder[j]) return false;
    }
  }
  for (int i = read.state.solvedCount; i < kGroups; i++) {
    if (read.state.solvedOrder[i] != 0) return false;
  }
  if (read.state.guessCount > MAX_GUESSES) return false;
  for (int i = 0; i < MAX_GUESSES; i++) {
    const bool used = i < read.state.guessCount;
    if (used && bitCount(read.state.guessed[i]) != kMembers) return false;
    if (!used && read.state.guessed[i] != 0) return false;
  }
  if (bitCount(read.selected) > kMembers) return false;
  for (int i = 0; i < read.state.solvedCount; i++) {
    if ((read.selected >> (read.state.solvedOrder[i] * kMembers)) & 0xF) return false;
  }

  board = read;
  return true;
}

}  // namespace connections
