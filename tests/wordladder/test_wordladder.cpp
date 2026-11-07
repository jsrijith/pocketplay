// Word Ladder with no panel, after touch-licorice-apps' test_wordladder.py: every stored ladder is
// a real ladder of listed words, the day's puzzle, playing a ladder, words that aren't listed or
// are already used, UNDO, HINT on and off the stored ladder, a long ladder's rows, every day kept,
// earlier and later days, and the save.
#include <cstdio>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "wordladder_flow.h"
#include "wordladder_rules.h"
#include "wordladder_save.h"

using namespace wordladder;

namespace {

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    failures++;
  }
}

std::vector<std::string> allWords(size_t n) {
  std::vector<std::string> out;
  int count;
  listFor(n, count);
  for (int i = 0; i < count; i++) out.push_back(wordAt(n, i));
  return out;
}

// Distances to the target over every word of its length.
std::map<std::string, int> distTo(const std::string& target) {
  std::map<std::string, int> d;
  std::deque<std::string> q;
  d[target] = 0;
  q.push_back(target);
  while (!q.empty()) {
    const std::string x = q.front();
    q.pop_front();
    for (size_t i = 0; i < x.size(); i++) {
      for (char c = 'a'; c <= 'z'; c++) {
        std::string y = x;
        y[i] = c;
        if (y != x && isWord(y) && !d.count(y)) {
          d[y] = d[x] + 1;
          q.push_back(y);
        }
      }
    }
  }
  return d;
}

void testData() {
  for (size_t n : {4u, 5u}) {
    const std::vector<std::string> words = allWords(n);
    bool sorted = true;
    for (size_t i = 1; i < words.size(); i++) sorted = sorted && words[i - 1] < words[i];
    check(sorted, "each word list is in order, for the search");
    bool found = true;
    for (const std::string& w : words) found = found && isWord(w);
    check(found, "and every word is found");
  }
  check(!isWord("zzzz") && !isWord("abc") && !isWord("cold!"), "nothing else is a word");

  bool ok = true;
  int five = 0;
  for (int k = 0; k < kPuzzleCount; k++) {
    const Puzzle p = puzzleFor(kStartDay + k);
    five += p.length == 5;
    if ((p.length != 4 && p.length != 5) || p.par < 4 || p.par > 6 || p.number != k + 1) ok = false;
    std::set<std::string> seen(p.path.begin(), p.path.end());
    if (seen.size() != p.path.size()) ok = false;
    for (size_t i = 0; i < p.path.size(); i++) {
      if (!isWord(p.path[i]) || p.path[i].size() != p.length) ok = false;
      if (i > 0 && !oneApart(p.path[i - 1], p.path[i])) ok = false;
    }
    // Par is the shortest ladder.
    if (k % 25 == 0 && distTo(p.target)[p.start] != p.par) ok = false;
  }
  check(ok, "all 730 ladders are listed words, one letter apart, 4 to 6 steps, none twice");
  check(five >= kPuzzleCount / 3 - 1 && five <= kPuzzleCount / 3 + 1, "every third has five letters");
}

void testDays() {
  int y, m, d;
  civilFromDays(kStartDay, y, m, d);
  check(y == 2026 && m == 11 && d == 1, "1 November 2026 is day 1");
  check(dateLabel(kStartDay) == "SUN 1 NOV" && dateLabel(kStartDay + 6) == "SAT 7 NOV", "dates read as SUN 1 NOV");
  check(dateLabel(daysFromCivil(2027, 2, 28) + 1) == "MON 1 MAR", "past the end of February");
  check(puzzleFor(kStartDay + 4).number == 5, "day 5 plays puzzle 5");
  check(puzzleFor(kStartDay + kPuzzleCount).number == 1, "and the list starts over after the last");
  check(puzzleFor(kStartDay - 1).number == kPuzzleCount, "a day before the first plays the last");
}

// Frames the letter that differs and presses its key.
Change stepTo(State& s, const std::string& w) {
  const std::string& cur = current(s.ladder);
  for (size_t i = 0; i < w.size(); i++) {
    if (w[i] != cur[i]) {
      choose(s, static_cast<int>(i));
      return pressKey(s, w[i]);
    }
  }
  return Change::None;
}

void testPlay() {
  State s;
  s.today = kStartDay + 4;
  openDay(s, s.today);
  const std::vector<std::string> path = s.ladder.puzzle.path;
  check(s.ladder.pos == firstOpen(s.ladder), "the frame starts on the first letter still to change");

  Change last = Change::None;
  for (size_t i = 1; i < path.size(); i++) last = stepTo(s, path[i]);
  check(last == Change::Solved && s.ladder.solved && s.message == Message::Solved, "the stored ladder, played, solves it");
  check(pressKey(s, 'a') == Change::None && hintTap(s) == Change::None, "nothing more once solved");
  check(s.days.count(s.today) && s.days[s.today].solved, "and the day is kept as solved");

  check(openMenu(s) == Change::View && startOverTap(s) == Change::NewLadder && s.ladder.steps.empty(), "START OVER");
  check(!s.days.count(s.today), "and an empty day isn't kept");

  // A word not in the list, a word already used, the same letter, UNDO.
  const std::string& start = path[0];
  std::string bad;
  for (size_t i = 0; i < start.size() && bad.empty(); i++) {
    for (char c : std::string("qxzjvk")) {
      std::string t = start;
      t[i] = c;
      if (!isWord(t)) {
        bad = t;
        break;
      }
    }
  }
  check(stepTo(s, bad) == Change::View && s.message == Message::NotWord && s.word == bad && s.ladder.steps.empty(),
        "a word not in the list is refused, saying so");
  check(stepTo(s, bad) == Change::None, "and again changes nothing");
  check(pressKey(s, start[static_cast<size_t>(s.ladder.pos)]) == Change::None, "the letter already there changes nothing");
  check(stepTo(s, path[1]) == Change::Move && s.ladder.steps.size() == 1, "a listed word joins the ladder");
  check(stepTo(s, path[0]) == Change::View && s.message == Message::Used, "the start can't be used again");
  check(undoTap(s) == Change::Move && s.ladder.steps.empty() && undoTap(s) == Change::None, "UNDO, and nothing to undo");

  // Up and Down move the frame, round at the ends.
  s.ladder.pos = 0;
  check(stepFrame(s, -1) == Change::View && s.ladder.pos == static_cast<int>(start.size()) - 1, "Up from the first letter goes to the last");
  check(stepFrame(s, 1) == Change::View && s.ladder.pos == 0, "and Down back round");
  check(choose(s, 0) == Change::None && choose(s, 9) == Change::None, "framing the framed letter, or none, changes nothing");

  // HINT on the stored ladder, then off it: always one step nearer.
  check(hintTap(s) == Change::Move && s.ladder.steps.size() == 1 && s.ladder.steps[0] == path[1] && s.word == path[1], "HINT on the stored ladder");
  check(hintCount(s.ladder) == 1 && (s.ladder.hinted & 1), "and it counts as a hint");
  undoTap(s);
  check(s.ladder.hinted == 0, "UNDO takes the hint back too");
  std::map<std::string, int> d = distTo(path.back());
  std::string off;
  for (const std::string& w : allWords(start.size())) {
    if (oneApart(w, start) && d.count(w) && d[w] >= d[start]) {
      bool onPath = false;
      for (const std::string& p : path) onPath = onPath || p == w;
      if (!onPath) {
        off = w;
        break;
      }
    }
  }
  check(!off.empty() && stepTo(s, off) == Change::Move, "a step off the stored ladder");
  bool nearer = true;
  for (int k = 0; k < 12 && !s.ladder.solved; k++) {
    const std::string before = current(s.ladder);
    hintTap(s);
    nearer = nearer && d[current(s.ladder)] == d[before] - 1;
  }
  check(nearer && s.ladder.solved, "HINT follows a shortest ladder from off it, to the end");

  Ladder fresh;
  wordladder::start(fresh, puzzleFor(kStartDay));
  check(nextWord(fresh) == fresh.puzzle.path[1], "the next word from the start is the stored ladder's");
}

// n more distinct words from cur, one letter apart, never the target.
bool walk(const std::string& cur, std::set<std::string>& used, const std::string& target, int n, std::vector<std::string>& out) {
  if (n == 0) return true;
  for (const std::string& w : allWords(cur.size())) {
    if (!used.count(w) && w != target && oneApart(w, cur)) {
      used.insert(w);
      out.push_back(w);
      if (walk(w, used, target, n - 1, out)) return true;
      out.pop_back();
      used.erase(w);
    }
  }
  return false;
}

void testLongAndKept() {
  State s;
  s.today = kStartDay + 4;
  openDay(s, s.today);
  std::set<std::string> used = {s.ladder.puzzle.start};
  std::vector<std::string> steps;
  check(walk(s.ladder.puzzle.start, used, s.ladder.puzzle.target, 12, steps), "a long way round");
  for (const std::string& w : steps) stepTo(s, w);
  check(s.ladder.steps.size() == 12, "twelve steps");
  const std::vector<Row> r = rows(s.ladder);
  check(r.size() == kRows && r[1].kind == RowKind::Gap && r[1].step == 8 && r[2].step == 8 && r.back().kind == RowKind::Target,
        "the newest four on screen, a row counting the eight above, and the target");
  check(newestSlot(s.ladder) == kRows - 2, "the newest word just above the target");
  Ladder few;
  start(few, s.ladder.puzzle);
  check(rows(few).size() == 2 && newestSlot(few) == 0, "a fresh ladder is the start and the target");

  // Saved and loaded back; earlier days keep their own ladder.
  std::vector<uint8_t> bytes = packDays(s);
  State back;
  back.today = s.today;
  check(unpackDays(bytes.data(), bytes.size(), back), "the days load back");
  openDay(back, back.today);
  check(back.ladder.steps == s.ladder.steps, "with today's twelve steps");
  check(earlierTap(back) == Change::NewLadder && back.day == back.today - 1 && back.ladder.steps.empty(), "EARLIER opens yesterday, fresh");
  check(back.ladder.puzzle.number == 4, "yesterday's puzzle");
  check(hintTap(back) == Change::Move && back.days.size() == 2, "and keeps its own");
  check(laterTap(back) == Change::NewLadder && back.ladder.steps.size() == 12, "LATER back to today");
  check(laterTap(back) == Change::None && todayTap(back) == Change::None, "no later than today");
  check(earlierTap(back) == Change::NewLadder && back.ladder.steps.size() == 1 && hintCount(back.ladder) == 1, "yesterday's hint kept");
  check(todayTap(back) == Change::NewLadder && back.day == back.today, "TODAY");

  // The newest 400 days are kept.
  State many;
  for (int k = 0; k < 410; k++) {
    openDay(many, kStartDay - 500 + k);
    hintTap(many);
  }
  check(many.days.size() == kMaxDays && many.days.begin()->first == kStartDay - 490, "only the newest 400 days");
  bytes = packDays(many);
  State manyBack;
  check(unpackDays(bytes.data(), bytes.size(), manyBack) && manyBack.days.size() == kMaxDays, "and all 400 load back");
}

void testSaveRefused() {
  State s;
  openDay(s, kStartDay);
  const std::vector<std::string> path = s.ladder.puzzle.path;
  stepTo(s, path[1]);
  stepTo(s, path[2]);
  const std::vector<uint8_t> good = packDays(s);
  State t;
  check(unpackDays(good.data(), good.size(), t) && t.days.size() == 1, "a good save loads");

  auto refused = [&](std::vector<uint8_t> bytes) {
    State u;
    u.days[5] = Record{{"x"}, 0, false};
    return !unpackDays(bytes.data(), bytes.size(), u) && u.days.size() == 1 && u.days.count(5);
  };
  std::vector<uint8_t> b = good;
  b[0] = SAVE_VERSION + 1;
  check(refused(b), "another version is refused, leaving the state alone");
  b = good;
  b.pop_back();
  check(refused(b), "a short one");
  b = good;
  b.push_back(0);
  check(refused(b), "a long one");
  b = good;
  b[b.size() - 1] = 'Q';
  check(refused(b), "a letter that isn't lower case");
  // The second step swapped for the first: listed, but not one letter on from it.
  b = good;
  const size_t len = path[0].size();
  for (size_t i = 0; i < len; i++) b[b.size() - len + i] = static_cast<uint8_t>(path[0][i]);
  check(refused(b), "a step that isn't one letter on");
  b = good;
  b[b.size() - 1] = 'q';
  check(refused(b) || isWord(std::string(b.end() - static_cast<long>(len), b.end())), "a step that isn't a word");
  b = good;
  b[6] = 1;  // solved, but the last step isn't the target
  check(refused(b), "solved without reaching the target");
  b = good;
  b[7] = 4;  // a hint bit past the last step
  check(refused(b), "a hint bit past the steps");
  b = good;
  b[5] = 0;  // no steps
  b.resize(8);
  b.pop_back();
  check(refused(b), "an empty day");
  // The same step twice: path[1], path[2], path[1].
  b = good;
  for (char c : path[1]) b.push_back(static_cast<uint8_t>(c));
  b[5] = 3;
  check(refused(b), "a word twice");
  std::vector<uint8_t> two = good;
  two[1] = 2;
  two.insert(two.end(), good.begin() + 3, good.end());
  check(refused(two), "the same day twice");
}

}  // namespace

int main() {
  testData();
  testDays();
  testPlay();
  testLongAndKept();
  testSaveRefused();
  if (failures) {
    std::printf("%d failure(s)\n", failures);
    return 1;
  }
  std::printf("wordladder: all passed\n");
  return 0;
}
