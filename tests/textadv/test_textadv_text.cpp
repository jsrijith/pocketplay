// The text adventure screen's pieces with no panel: wrapping and paging the transcript, the word
// chips from a reply, building a command, story file names and the settings save. Chips are
// checked against Zork I's own dictionary, through the interpreter.
#include <cstdio>
#include <string>
#include <vector>

#include "textadv_text.h"
#include "zmachine.h"
#include "zork_stories.h"

using namespace textadv;

namespace {

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    failures++;
  }
}

// Ten pixels a letter, so a width of 100 holds ten.
const Measure measure = [](const std::string& s) { return static_cast<int>(s.size()) * 10; };

void testWrap() {
  Transcript t;
  t.add("The quick brown fox jumps over the lazy dog.\n", measure, 100);
  check(t.size() == 5 && t.lines()[0] == "The quick" && t.lines()[1] == "brown fox" && t.lines()[2] == "jumps over" &&
            t.lines()[3] == "the lazy" && t.lines()[4] == "dog.",
        "wrapped at spaces, a line that fills exactly kept whole");
  t.clear();
  t.add("Supercalifragilistic\n", measure, 100);
  check(t.size() == 2 && t.lines()[0] == "Supercalif" && t.lines()[1] == "ragilistic", "a word too long breaks mid-word");
  t.clear();
  t.add("West of House\n\n>", measure, 200);
  t.add("open mailbox\n", measure, 200);
  check(t.size() == 3 && t.lines()[1].empty() && t.lines()[2] == ">open mailbox", "the prompt and the command share a line");
  t.add("Opened.\n", measure, 200);
  check(t.size() == 4 && t.lines()[3] == "Opened.", "and the reply starts on the next");

  Transcript w;
  w.add("The quick brown fox jumps over the lazy dog.\nNext.\n", measure, 100);
  w.rewrap(measure, 200);
  check(w.size() == 4 && w.lines()[0] == "The quick brown fox" && w.lines()[1] == "jumps over the lazy" && w.lines()[2] == "dog.",
        "a wider rewrap joins the wrapper's breaks back up");
  w.rewrap(measure, 100);
  check(w.size() == 6 && w.lines()[5] == "Next.", "and the paragraph breaks stay");

  t.rewrap(measure, 70);
  check(t.lines()[0] == "West of" && t.lines()[3] == ">open" && t.lines()[4] == "mailbox", "a new size rewraps everything");
}

void testPaging() {
  Transcript t;
  for (int i = 0; i < 30; i++) t.add("line " + std::to_string(i) + "\n", measure, 200);
  t.showEnd(10);
  check(t.top() == 20, "the end shows the last page");
  check(t.page(-1, 10) && t.top() == 10, "a page back");
  check(t.page(-1, 10) && t.top() == 0 && !t.page(-1, 10), "and back to the start, no further");
  check(t.page(1, 10) && t.page(1, 10) && t.top() == 20 && !t.page(1, 10), "and on to the end, no further");

  // A long reply opens at its start; a short one shows the end.
  const size_t before = t.size();
  for (int i = 0; i < 15; i++) t.add("reply " + std::to_string(i) + "\n", measure, 200);
  t.showReply(before, 10);
  check(t.top() == before, "a long reply is shown from its start");
  const size_t again = t.size();
  t.add("Ok.\n", measure, 200);
  t.showReply(again, 10);
  check(t.top() == t.size() - 10, "a short one with what's above it");

  for (int i = 0; i < 500; i++) t.add("more\n", measure, 200);
  check(t.size() == Transcript::kMaxLines, "the oldest lines drop off past 400");

  Transcript u;
  u.setLines(t.tail(3));
  check(u.size() == 3 && u.lines()[2] == "more", "the tail saves and loads back");
}

void testChips() {
  zmachine::Machine m;
  std::string error;
  m.load(ZORK_I, sizeof(ZORK_I), error);
  auto known = [&](const std::string& w) { return m.lookup(w) != 0; };
  std::vector<std::string> words;
  std::vector<std::string> chips = makeChips("You are standing in an open field west of a white house, with a boarded front door.\n"
                                             "There is a small mailbox here.",
                                             words, known);
  auto at = [&](const std::string& w) {
    for (size_t i = 0; i < chips.size(); i++) {
      if (chips[i] == w) return static_cast<int>(i);
    }
    return -1;
  };
  check(at("mailbox") >= 0 && at("house") >= 0 && at("door") >= 0, "the game's words become chips");
  check(at("standing") < 0 || m.lookup("standing") != 0, "only words the game knows");
  check(at("the") < 0 && at("there") < 0, "never the little words");
  check(at("open") == static_cast<int>(chips.size()) - 23 + 3, "a verb in the reply stays with the verbs, not twice");
  check(chips.back() == "all" && at("mailbox") < at("take"), "the game's words first, then the verbs");

  chips = makeChips("What do you want to put?", words, known);
  check(at("mailbox") >= 0, "a short reply keeps the room's words");

  std::string cmd;
  addWord(cmd, "open", 60);
  addWord(cmd, "mailbox", 60);
  check(cmd == "open mailbox", "chips build a command");
  check(deleteWord(cmd) && cmd == "open" && deleteWord(cmd) && cmd.empty() && !deleteWord(cmd), "DEL takes the last word");
  std::string full(58, 'x');
  addWord(full, "door", 60);
  check(full.size() == 58, "a word that won't fit isn't added");
  check(withoutEcho("open mailbox\nOpening the mailbox.") == "Opening the mailbox.", "the echoed command isn't a chip source");
}

void testNames() {
  std::string base;
  check(isStoryFile("Zork I.z3", base) && base == "Zork I", "a .z3 is a story");
  check(isStoryFile("curses.Z5", base) && base == "curses", "any case");
  check(isStoryFile("anchor.z8", base) && isStoryFile("x.z4", base) && isStoryFile("y.z7", base), "versions 4, 7 and 8");
  check(!isStoryFile("arthur.z6", base) && !isStoryFile("game.ulx", base) && !isStoryFile("._zork.z3", base), "not v6, Glulx or hidden files");
  check(keyOf("Zork I.z3") == keyOf("Zork I.z3") && keyOf("Zork I.z3") != keyOf("Zork II.z3"), "each file its own key");
}

void testSettings() {
  uint8_t data[2 + kMaxFileName];
  const size_t size = packSettings(true, "Zork II.z3", data);
  bool small = false;
  std::string last;
  check(unpackSettings(SETTINGS_VERSION, data, size, small, last) && small && last == "Zork II.z3", "the settings round-trip");
  check(!unpackSettings(SETTINGS_VERSION + 1, data, size, small, last), "another version is refused");
  check(!unpackSettings(SETTINGS_VERSION, data, size - 1, small, last), "a short one is refused");
  data[0] = 2;
  check(!unpackSettings(SETTINGS_VERSION, data, size, small, last), "a size that isn't 0 or 1 is refused");
}

}  // namespace

int main() {
  testWrap();
  testPaging();
  testChips();
  testNames();
  testSettings();
  if (failures) {
    std::printf("%d failure(s)\n", failures);
    return 1;
  }
  std::printf("textadv text: all passed\n");
  return 0;
}
