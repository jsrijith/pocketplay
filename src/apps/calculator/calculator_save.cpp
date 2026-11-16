#include "calculator_save.h"

#include <cmath>
#include <cstring>

namespace calculator {

namespace {

constexpr uint8_t FLAG_FRESH = 1;
constexpr uint8_t FLAG_ERROR = 2;

// A double's bits, least significant byte first.
void putNumber(uint8_t* out, double value) {
  uint64_t bits = 0;
  memcpy(&bits, &value, sizeof(bits));
  for (int i = 0; i < 8; i++) out[i] = static_cast<uint8_t>(bits >> (8 * i));
}

double getNumber(const uint8_t* in) {
  uint64_t bits = 0;
  for (int i = 0; i < 8; i++) bits |= static_cast<uint64_t>(in[i]) << (8 * i);
  double value = 0;
  memcpy(&value, &bits, sizeof(value));
  return value;
}

bool validOp(uint8_t value) { return value < OP_COUNT; }

}  // namespace

size_t pack(const State& state, uint8_t* out) {
  size_t at = 0;
  const size_t length = strlen(state.entry);
  out[at++] = static_cast<uint8_t>(length);
  memset(out + at, 0, MAX_ENTRY);
  memcpy(out + at, state.entry, length);
  at += MAX_ENTRY;
  putNumber(out + at, state.accumulator);
  at += 8;
  putNumber(out + at, state.lastOperand);
  at += 8;
  out[at++] = static_cast<uint8_t>(state.pending);
  out[at++] = static_cast<uint8_t>(state.lastOp);
  out[at++] = static_cast<uint8_t>((state.fresh ? FLAG_FRESH : 0) | (state.error ? FLAG_ERROR : 0));
  return at;
}

bool unpack(uint8_t version, const uint8_t* data, size_t size, State& state) {
  if (version != SAVE_VERSION || size != SAVE_BYTES) return false;
  size_t at = 0;
  const uint8_t length = data[at++];
  if (length == 0 || length > MAX_ENTRY) return false;

  State loaded;
  memcpy(loaded.entry, data + at, length);
  loaded.entry[length] = '\0';
  if (!validEntry(loaded.entry)) return false;
  at += MAX_ENTRY;

  loaded.accumulator = getNumber(data + at);
  at += 8;
  loaded.lastOperand = getNumber(data + at);
  at += 8;
  if (!std::isfinite(loaded.accumulator) || !std::isfinite(loaded.lastOperand)) return false;
  if (std::fabs(loaded.accumulator) >= LIMIT || std::fabs(loaded.lastOperand) >= LIMIT) return false;

  const uint8_t pending = data[at++];
  const uint8_t lastOp = data[at++];
  if (!validOp(pending) || !validOp(lastOp)) return false;
  loaded.pending = static_cast<Op>(pending);
  loaded.lastOp = static_cast<Op>(lastOp);

  const uint8_t flags = data[at++];
  if ((flags & ~(FLAG_FRESH | FLAG_ERROR)) != 0) return false;
  loaded.fresh = (flags & FLAG_FRESH) != 0;
  loaded.error = (flags & FLAG_ERROR) != 0;

  state = loaded;
  return true;
}

}  // namespace calculator
