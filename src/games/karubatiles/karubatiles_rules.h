#pragma once

#include <cstddef>
#include <cstdint>

// Karuba Tiles: draws the 36 jungle tiles of the board game Karuba for the player leading the
// round, one at a time, at random from those not drawn yet. No screen or card, so it can be tested
// off the device: the screen hands each draw its entropy, and nothing here reaches for hardware.
namespace karubatiles {

constexpr int kTiles = 36;

// Which edges a tile's paths reach, as printed with its number upright.
constexpr uint8_t kNorth = 1;
constexpr uint8_t kEast = 2;
constexpr uint8_t kSouth = 4;
constexpr uint8_t kWest = 8;

enum class Treasure : uint8_t { None, Crystal, Gold };

struct Tile {
  uint8_t paths;  // kNorth | kEast | kSouth | kWest
  Treasure treasure;
};

// Tiles 1 to 36: the paths and treasure on each, read from the printed tiles.
constexpr uint8_t NS = kNorth | kSouth;
constexpr uint8_t EW = kEast | kWest;
constexpr uint8_t ALL = kNorth | kEast | kSouth | kWest;
constexpr Treasure NO = Treasure::None;
constexpr Treasure CR = Treasure::Crystal;
constexpr Treasure GO = Treasure::Gold;

constexpr Tile kTileSet[kTiles] = {
    {EW, NO},                      // 1
    {EW, NO},                      // 2
    {kEast | kSouth, CR},          // 3
    {kSouth | kWest, CR},          // 4
    {kEast | kSouth | kWest, NO},  // 5
    {kEast | kSouth | kWest, NO},  // 6
    {ALL, NO},                     // 7
    {ALL, NO},                     // 8
    {kNorth | kEast, CR},          // 9
    {kNorth | kWest, CR},          // 10
    {kNorth | kEast | kWest, CR},  // 11
    {kNorth | kEast | kWest, NO},  // 12
    {EW, CR},                      // 13
    {EW, GO},                      // 14
    {EW, CR},                      // 15
    {EW, GO},                      // 16
    {NS, NO},                      // 17
    {NS, NO},                      // 18
    {EW, NO},                      // 19
    {EW, NO},                      // 20
    {kEast | kSouth, NO},          // 21
    {kSouth | kWest, NO},          // 22
    {kNorth | kEast | kSouth, NO}, // 23
    {kNorth | kSouth | kWest, NO}, // 24
    {ALL, NO},                     // 25
    {ALL, NO},                     // 26
    {kNorth | kEast, CR},          // 27
    {kNorth | kWest, NO},          // 28
    {kNorth | kEast | kSouth, NO}, // 29
    {kNorth | kSouth | kWest, NO}, // 30
    {NS, CR},                      // 31
    {NS, GO},                      // 32
    {NS, CR},                      // 33
    {NS, GO},                      // 34
    {NS, NO},                      // 35
    {NS, NO},                      // 36
};

inline const Tile& tile(int number) { return kTileSet[number - 1]; }

struct State {
  uint8_t count = 0;            // how many have been drawn
  uint8_t order[kTiles] = {};   // the draws, oldest first: 1 to 36, no repeats
};

inline bool finished(const State& s) { return s.count >= kTiles; }
inline int last(const State& s) { return s.count == 0 ? 0 : s.order[s.count - 1]; }

inline bool isDrawn(const State& s, int number) {
  for (int i = 0; i < s.count; i++) {
    if (s.order[i] == number) return true;
  }
  return false;
}

// SplitMix64's finaliser: every input bit flips about half the output bits, so entropy that
// arrives in a few low bits, such as a tap's timing, spreads through the whole state.
inline uint64_t mix(uint64_t z) {
  z += 0x9E3779B97F4A7C15ull;
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
  return z ^ (z >> 31);
}

// The random state, kept between draws. Each draw stirs in fresh entropy first, so it never
// depends on one source alone: the chip's random number and the microsecond of the tap.
struct Source {
  uint64_t state = 0;
  uint64_t counter = 0;

  void stir(uint64_t entropy) { state = mix(state ^ mix(entropy)); }

  uint32_t next() {
    counter++;
    return static_cast<uint32_t>(mix(state + counter * 0xD1B54A32D192ED03ull) >> 32);
  }
};

// 0 to n-1, all exactly equally likely: values above the largest multiple of n that fits in
// 32 bits are thrown back, so no result gets the leftover.
template <typename Next>
uint32_t uniform(uint32_t n, Next&& next) {
  const uint32_t limit = 0xFFFFFFFFu - (0xFFFFFFFFu % n + 1) % n;
  uint32_t v;
  do {
    v = next();
  } while (v > limit);
  return v % n;
}

// Draws the next tile: one of those left, each equally likely. Returns false, changing nothing,
// once every tile has been drawn.
inline bool draw(State& s, Source& source, uint64_t entropy) {
  if (finished(s)) return false;
  uint8_t left[kTiles];
  int n = 0;
  for (int number = 1; number <= kTiles; number++) {
    if (!isDrawn(s, number)) left[n++] = static_cast<uint8_t>(number);
  }
  source.stir(entropy);
  s.order[s.count++] = left[uniform(static_cast<uint32_t>(n), [&] { return source.next(); })];
  return true;
}

}  // namespace karubatiles
