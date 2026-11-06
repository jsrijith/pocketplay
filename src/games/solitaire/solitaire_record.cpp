#include "solitaire_record.h"

namespace solitaire {

void appendResult(std::vector<uint8_t>& results, bool won) {
  if (results.size() >= MAX_RESULTS) results.erase(results.begin(), results.begin() + (results.size() - MAX_RESULTS + 1));
  results.push_back(won ? RESULT_WON : RESULT_LOST);
}

bool resultsValid(const uint8_t* data, size_t size) {
  if (size > MAX_RESULTS) return false;
  for (size_t i = 0; i < size; i++) {
    if (data[i] != RESULT_LOST && data[i] != RESULT_WON) return false;
  }
  return true;
}

Record summarise(const std::vector<uint8_t>& results) {
  Record record;
  const int count = static_cast<int>(results.size());
  record.played = count;
  for (const uint8_t result : results) {
    if (result == RESULT_WON) record.wins++;
  }
  for (int i = count - 1; i >= 0 && results[i] == RESULT_WON; i--) record.streak++;
  for (int i = 0; i < RECENT_GAMES; i++) {
    const int index = count - RECENT_GAMES + i;
    if (index >= 0) record.recent[i] = results[index];
  }
  return record;
}

}  // namespace solitaire
