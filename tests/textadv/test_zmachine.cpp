// The Z-machine interpreter, checked by playing the built-in Zorks the way touch-licorice-apps'
// test_textadv.py plays Zork I: the opening, commands and their replies, the score, the status
// line, the dictionary, the autosave snapshot, and the game's own SAVE and RESTORE. Then the two
// interpreter tests the source runs too, CZECH and Praxix, cover versions 4 to 8.
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "zmachine.h"
#include "zork_stories.h"

using zmachine::Machine;
using zmachine::Result;

namespace {

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    failures++;
  }
}

bool has(const std::string& text, const char* part) { return text.find(part) != std::string::npos; }

// Runs until the game wants input, and returns what it printed.
std::string runToInput(Machine& m, Result* last = nullptr) {
  Result r = Result::More;
  for (int k = 0; k < 1000 && r == Result::More; k++) r = m.run(10000);
  if (last) *last = r;
  return m.takeOutput();
}

std::string say(Machine& m, const char* line) {
  m.sendLine(line);
  return runToInput(m);
}

void testZorkI() {
  Machine m;
  std::string error;
  check(m.load(textadv::ZORK_I, sizeof(textadv::ZORK_I), error), "Zork I loads");
  m.seed(12345);
  Result r;
  const std::string opening = runToInput(m, &r);
  check(r == Result::Input, "and stops for input");
  check(has(opening, "ZORK I: The Great Underground Empire"), "with its title");
  check(has(opening, "West of House"), "west of the house");

  std::string left, right;
  m.status(left, right);
  check(left == "West of House" && right == "SCORE 0   MOVES 0", "the status line has the room, the score and the moves");

  std::string out = say(m, "open mailbox");
  check(has(out, "open mailbox") && has(out, "Opening the small mailbox reveals a leaflet."), "open mailbox, echoed");
  out = say(m, "TAKE LEAFLET");
  check(has(out, "Taken."), "commands are taken in any case");
  out = say(m, "read leaflet");
  check(has(out, "WELCOME TO ZORK!"), "read leaflet");

  check(m.lookup("mailbox") != 0 && m.lookup("leaflet") != 0 && m.lookup("xyzzyq") == 0, "the dictionary knows the game's words");

  // The autosave: a snapshot at the read, into a fresh machine.
  std::vector<uint8_t> snap;
  check(m.snapshot(snap), "a snapshot at the read");
  Machine back;
  back.load(textadv::ZORK_I, sizeof(textadv::ZORK_I), error);
  check(back.restore(snap.data(), snap.size()), "restores into a fresh machine");
  runToInput(back);
  out = say(back, "inventory");
  check(has(out, "leaflet"), "and still has the leaflet");
  back.status(left, right);
  check(right == "SCORE 0   MOVES 4", "and the three moves before it, with inventory the fourth");

  // Around the house and in through the window: ten points.
  say(m, "n");
  say(m, "e");
  out = say(m, "open window");
  check(has(out, "open the window"), "open the window");
  out = say(m, "in");
  check(has(out, "Kitchen"), "into the kitchen");
  m.status(left, right);
  check(left == "Kitchen" && right == "SCORE 10   MOVES 7", "ten points for getting in");

  // The game's own SAVE and RESTORE, in one slot.
  std::vector<uint8_t> slot;
  m.onSave = [&](const std::vector<uint8_t>& s) {
    slot = s;
    return true;
  };
  m.onRestore = [&](std::vector<uint8_t>& s) {
    if (slot.empty()) return false;
    s = slot;
    return true;
  };
  out = say(m, "save");
  check(has(out, "Ok."), "SAVE says Ok");
  say(m, "w");
  m.status(left, right);
  check(left == "Living Room", "west into the living room");
  out = say(m, "restore");
  check(has(out, "Ok."), "RESTORE says Ok");
  m.status(left, right);
  check(left == "Kitchen", "and puts us back in the kitchen");

  // A snapshot from another game is refused.
  Machine two;
  two.load(textadv::ZORK_II, sizeof(textadv::ZORK_II), error);
  check(!two.restore(snap.data(), snap.size()), "a Zork I save won't go into Zork II");
  check(!back.restore(snap.data(), snap.size() - 1), "a short snapshot is refused");
  std::vector<uint8_t> bad = snap;
  bad[0] ^= 1;
  check(!back.restore(bad.data(), bad.size()), "so is one with the wrong mark");
}

void testZorkIIandIII() {
  std::string error;
  Machine two;
  check(two.load(textadv::ZORK_II, sizeof(textadv::ZORK_II), error), "Zork II loads");
  std::string out = runToInput(two);
  check(has(out, "ZORK II") && has(out, "Inside the Barrow"), "and opens in the barrow");
  out = say(two, "look");
  check(has(out, "Inside the Barrow"), "look");

  Machine three;
  check(three.load(textadv::ZORK_III, sizeof(textadv::ZORK_III), error), "Zork III loads");
  out = runToInput(three);
  check(has(out, "ZORK III") && has(out, "Endless Stair"), "and opens on the endless stair");
}

void testRefused() {
  Machine m;
  std::string error;
  uint8_t tiny[10] = {3};
  check(!m.load(tiny, sizeof(tiny), error) && !error.empty(), "a file too short to be a story is refused");
  std::vector<uint8_t> v6(textadv::ZORK_I, textadv::ZORK_I + sizeof(textadv::ZORK_I));
  v6[0] = 6;
  check(!m.load(v6.data(), v6.size(), error) && error == "STORY VERSION 6 ISN'T SUPPORTED.", "version 6 is refused, saying why");
}

std::vector<uint8_t> readFile(const char* path) {
  std::vector<uint8_t> data;
  FILE* f = std::fopen(path, "rb");
  if (!f) return data;
  int c;
  while ((c = std::fgetc(f)) != EOF) data.push_back(static_cast<uint8_t>(c));
  std::fclose(f);
  return data;
}

// Plays a test story to the end: a key answers every read_char, and `line` every read.
std::string playTest(const char* path, const char* line, Result& last) {
  const std::vector<uint8_t> story = readFile(path);
  Machine m;
  std::string error;
  std::string all;
  if (story.empty() || !m.load(story.data(), story.size(), error)) {
    last = Result::Error;
    return all;
  }
  m.seed(7);
  bool sent = false;
  for (int turn = 0; turn < 50; turn++) {
    last = Result::More;
    for (int k = 0; k < 100000 && last == Result::More; k++) last = m.run(10000);
    all += m.takeOutput();
    if (last == Result::Char) {
      m.sendChar(13);
    } else if (last == Result::Input && !sent) {
      m.sendLine(line);
      sent = true;
    } else {
      break;
    }
  }
  return all;
}

void testCzech() {
  Result last;
  const std::string out = playTest("czech.z5", "", last);
  check(last == Result::Quit, "CZECH runs to the end");
  check(has(out, "Failed: 0") && has(out, "Didn't crash: hooray!"), "and fails none of its tests");
}

void testPraxix() {
  Result last;
  const std::string out = playTest("praxix.z5", "all", last);
  check(has(out, "All tests passed."), "Praxix passes all its tests");
}

}  // namespace

int main() {
  testZorkI();
  testZorkIIandIII();
  testRefused();
  testCzech();
  testPraxix();
  if (failures) {
    std::printf("%d failure(s)\n", failures);
    return 1;
  }
  std::printf("zmachine: all passed\n");
  return 0;
}
