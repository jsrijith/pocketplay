// Crown & Crest with no panel: faces stay 1 to 6 and none is favoured on any of the four dice,
// even from the weakest entropy, the rejection step throws back the values that would favour
// some, the face art is all there, and the save.
#include <cstdio>
#include <vector>

#include "crowncrest_faces.h"
#include "crowncrest_rules.h"
#include "crowncrest_save.h"

using namespace crowncrest;

namespace {

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    failures++;
  }
}

double chiSquared(const std::vector<long>& counts, double expected) {
  double sum = 0;
  for (long c : counts) {
    const double d = c - expected;
    sum += d * d / expected;
  }
  return sum;
}

void testRejection() {
  check(kLimit == 4294967291u && (static_cast<uint64_t>(kLimit) + 1) % kFaces == 0, "the limit keeps a whole number of each face");
  const uint32_t values[] = {0xFFFFFFFFu, 0xFFFFFFFCu, kLimit};
  int at = 0;
  const uint8_t f = face([&] { return values[at++]; });
  check(at == 3 && f == kLimit % kFaces + 1, "values past the limit are thrown back, and the limit itself kept");
}

void testFair() {
  // The weakest entropy there could be: 0, 1, 2 and so on, as a tap timed in whole
  // microseconds with the chip's number stuck. The mixing must still give fair dice.
  constexpr long kRolls = 600000;
  Source source;
  std::vector<std::vector<long>> faces(kDice, std::vector<long>(kFaces, 0));
  std::vector<long> pairs(kFaces * kFaces, 0);
  bool inRange = true;
  for (long i = 0; i < kRolls; i++) {
    Roll r;
    roll(r, source, static_cast<uint64_t>(i));
    for (int d = 0; d < kDice; d++) {
      inRange = inRange && r.face[d] >= 1 && r.face[d] <= kFaces;
      if (r.face[d] >= 1 && r.face[d] <= kFaces) faces[d][r.face[d] - 1]++;
    }
    if (r.face[0] >= 1 && r.face[0] <= kFaces && r.face[3] >= 1 && r.face[3] <= kFaces) {
      pairs[(r.face[0] - 1) * kFaces + r.face[3] - 1]++;
    }
  }
  check(inRange, "every face is 1 to 6");
  // 99.9th percentiles: 20.5 for 5 degrees of freedom, 66.6 for 35.
  for (int d = 0; d < kDice; d++) {
    check(chiSquared(faces[d], kRolls / 6.0) < 20.5, "each die's faces come up evenly");
  }
  check(chiSquared(pairs, kRolls / 36.0) < 66.6, "the first and last dice don't follow each other");
}

void testFaces() {
  using namespace crowncrestFaces;
  check(FACE == 180 && ROW_BYTES == 23, "faces are 180 x 180");
  check(sizeof(FACES) == static_cast<size_t>(kDice * kFaces * ROW_BYTES * FACE), "24 faces");
  for (int d = 0; d < kDice; d++) {
    for (int f = 0; f < kFaces; f++) {
      long ink = 0;
      for (int i = 0; i < ROW_BYTES * FACE; i++) {
        for (int b = 0; b < 8; b++) ink += (FACES[d][f][i] & (0x80 >> b)) == 0;
      }
      // Padding bits are white, so ink is only the art: some, but far from a solid square.
      check(ink > 500 && ink < FACE * FACE / 2, "every face has art on it");
    }
  }
}

void testSave() {
  Roll r;
  Source source;
  roll(r, source, 42);
  uint8_t data[SAVE_BYTES];
  check(pack(r, data) == SAVE_BYTES, "pack writes four bytes");
  Roll back;
  check(unpack(SAVE_VERSION, data, SAVE_BYTES, back), "a save comes back");
  bool same = true;
  for (int d = 0; d < kDice; d++) same = same && back.face[d] == r.face[d];
  check(same, "the round trip keeps every die");

  Roll untouched;
  check(!unpack(SAVE_VERSION + 1, data, SAVE_BYTES, untouched), "a wrong version is refused");
  check(!unpack(SAVE_VERSION, data, SAVE_BYTES - 1, untouched), "a short save is refused");
  const uint8_t zero[SAVE_BYTES] = {1, 2, 0, 4};
  check(!unpack(SAVE_VERSION, zero, SAVE_BYTES, untouched), "a blank die is refused");
  const uint8_t seven[SAVE_BYTES] = {1, 7, 3, 4};
  check(!unpack(SAVE_VERSION, seven, SAVE_BYTES, untouched), "a face past 6 is refused");
  check(!rolled(untouched), "a refused save leaves the dice alone");
}

}  // namespace

int main() {
  testRejection();
  testFair();
  testFaces();
  testSave();
  if (failures) {
    std::printf("crowncrest: %d failed\n", failures);
    return 1;
  }
  std::printf("crowncrest: all passed\n");
  return 0;
}
