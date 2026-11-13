#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "thinkalike_deck.h"
#include "thinkalike_questions.h"
#include "thinkalike_save.h"

using namespace thinkalike;

namespace {

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    failures++;
  }
}

Questions parseText(const std::string& text) {
  return parse(reinterpret_cast<const uint8_t*>(text.data()), text.size());
}

void testParse() {
  Questions q = parseText("# a comment\n\nWhat is red?\r\n   Name a dog.  \n  # indented comment\nlast line");
  check(q.list.size() == 3, "comments and blank lines are skipped");
  check(q.list.size() == 3 && q.list[0] == "WHAT IS RED?", "CRLF is stripped and letters are capitals");
  check(q.list.size() == 3 && q.list[1] == "NAME A DOG.", "spaces round a line are trimmed");
  check(q.list.size() == 3 && q.list[2] == "LAST LINE", "a last line without a newline counts");
  check(q.tooLong == 0 && q.overLimit == 0, "nothing skipped");

  check(parseText("").list.empty(), "an empty file has no questions");
  check(parseText("\n\n# only\n   \n").list.empty(), "a file of comments has no questions");

  q = parseText("\xEF\xBB\xBF" "It\xE2\x80\x99" "s \xE2\x80\x9C" "fun\xE2\x80\x9D \xE2\x80\x94 really\xE2\x80\xA6 caf\xC3\xA9\tok\n");
  check(q.list.size() == 1 && q.list[0] == "IT'S \"FUN\" - REALLY... CAF OK", "UTF-8 punctuation becomes ASCII, other bytes go");

  q = parseText("\xE2\x80");  // a cut-off character at the end
  check(q.list.empty(), "a cut-off UTF-8 character is dropped");

  std::string longLine(MAX_QUESTION_CHARS, 'A');
  q = parseText(longLine + "\n" + longLine + "B\n");
  check(q.list.size() == 1 && q.tooLong == 1, "a line over the limit is left out and counted");

  std::string many;
  for (int i = 0; i < MAX_QUESTIONS + 3; i++) many += "Q" + std::to_string(i) + "\n";
  q = parseText(many);
  check(static_cast<int>(q.list.size()) == MAX_QUESTIONS && q.overLimit == 3, "questions past the limit are counted");

  const std::string a = "one\ntwo\n";
  const std::string b = "one\ntwo!\n";
  check(checksum(reinterpret_cast<const uint8_t*>(a.data()), a.size()) !=
            checksum(reinterpret_cast<const uint8_t*>(b.data()), b.size()),
        "an edit changes the checksum");
}

void testShippedFile() {
  FILE* f = std::fopen(QUESTIONS_FILE, "rb");
  check(f != nullptr, "questions.txt is there");
  if (!f) return;
  std::vector<uint8_t> data;
  int c;
  while ((c = std::fgetc(f)) != EOF) data.push_back(static_cast<uint8_t>(c));
  std::fclose(f);
  check(data.size() <= MAX_FILE_BYTES, "questions.txt fits the size limit");
  const Questions q = parse(data.data(), data.size());
  check(q.list.size() == 5 && q.tooLong == 0, "questions.txt holds five questions");
}

bool isPermutation(const std::vector<uint16_t>& order, int count) {
  if (static_cast<int>(order.size()) != count) return false;
  std::vector<bool> seen(count, false);
  for (uint16_t i : order) {
    if (i >= count || seen[i]) return false;
    seen[i] = true;
  }
  return true;
}

void testDeck() {
  for (int count : {1, 2, 5, 110, MAX_QUESTIONS}) {
    const Deck deck = shuffled(12345u, static_cast<uint16_t>(count));
    check(isPermutation(order(deck), count), "the order uses every question once");
    check(deck.drawn == 1, "a shuffle shows the first question");
  }
  check(order(shuffled(7u, 50)) == order(shuffled(7u, 50)), "a seed always gives the same order");
  check(order(shuffled(7u, 50)) != order(shuffled(8u, 50)), "another seed gives another order");
  check(shuffled(0u, 5).seed != 0, "a zero seed is replaced");

  Deck deck = shuffled(99u, 5);
  int shown = 1;
  while (next(deck)) shown++;
  check(shown == 5 && finished(deck), "five questions, then finished");
  check(!next(deck) && deck.drawn == 5, "next does nothing once finished");
}

void testSave() {
  Saved saved;
  saved.deck = shuffled(0xDEADBEEFu, 700);
  saved.deck.drawn = 300;
  saved.fileSum = 0x12345678u;
  uint8_t data[SAVE_BYTES];
  check(pack(saved, data) == SAVE_BYTES, "pack fills the save");

  Saved back;
  check(unpack(SAVE_VERSION, data, SAVE_BYTES, back), "a packed save unpacks");
  check(back.deck.seed == saved.deck.seed && back.deck.count == 700 && back.deck.drawn == 300 &&
            back.fileSum == 0x12345678u,
        "the save round-trips");

  Saved untouched;
  check(!unpack(SAVE_VERSION + 1, data, SAVE_BYTES, untouched), "another version is rejected");
  check(!unpack(SAVE_VERSION, data, SAVE_BYTES - 1, untouched), "a short save is rejected");

  uint8_t bad[SAVE_BYTES];
  std::memcpy(bad, data, sizeof(bad));
  bad[6] = 0;
  bad[7] = 0;
  check(!unpack(SAVE_VERSION, bad, SAVE_BYTES, untouched), "drawn 0 is rejected");
  std::memcpy(bad, data, sizeof(bad));
  bad[6] = 0xBD;  // 701 > count
  bad[7] = 0x02;
  check(!unpack(SAVE_VERSION, bad, SAVE_BYTES, untouched), "drawn past the count is rejected");
  std::memcpy(bad, data, sizeof(bad));
  bad[4] = 0xE9;  // 1001
  bad[5] = 0x03;
  check(!unpack(SAVE_VERSION, bad, SAVE_BYTES, untouched), "a count past the limit is rejected");
  std::memcpy(bad, data, sizeof(bad));
  std::memset(bad, 0, 4);
  check(!unpack(SAVE_VERSION, bad, SAVE_BYTES, untouched), "a zero seed is rejected");
  check(untouched.deck.count == 0, "a rejected save leaves the deck alone");
}

}  // namespace

int main() {
  testParse();
  testShippedFile();
  testDeck();
  testSave();
  if (failures != 0) {
    std::printf("%d failures\n", failures);
    return 1;
  }
  std::printf("thinkalike: all passed\n");
  return 0;
}
