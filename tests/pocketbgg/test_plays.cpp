// PocketBGG's plays, for last-played dates: a real page cut down to eight plays (sample_plays.xml,
// with the players and comments removed), and made-up pages for the cases it doesn't have.
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "pocketbgg_plays.h"

using namespace pocketbgg;

namespace {

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    failures++;
  }
}

std::string readSample() {
  std::ifstream file("sample_plays.xml", std::ios::binary);
  std::stringstream text;
  text << file.rdbuf();
  return text.str();
}

PlaysResult parse(const std::string& xml, PlaysPage& out) { return parsePlaysPage(xml.data(), xml.size(), out); }

Game game(uint32_t id, uint32_t lastPlayed = 0) {
  Game g;
  g.id = id;
  g.name = "G";
  g.lastPlayed = lastPlayed;
  return g;
}

void testSample() {
  const std::string xml = readSample();
  check(!xml.empty(), "the sample file is there");
  PlaysPage page;
  check(parse(xml, page) == PlaysResult::Ok, "the real page parses");
  check(page.total == 3396 && playsPages(page.total) == 34, "3396 plays, on 34 pages");
  check(page.plays.size() == 7, "seven plays: the one with no date is left out");
  check(page.plays.size() == 7 && page.plays[0].gameId == 266083 && page.plays[0].date == 20261117, "newest first");

  LastPlayed latest;
  addPlays(page, latest);
  check(latest.count(230802) == 1 && latest[230802] == 20261108, "Azul, played twice, keeps its later date");
  check(latest.count(1219) == 0, "the game whose play has no date isn't given one");
  check(latest.size() == 6, "six games played");

  // Pages arriving in any order give the same answer.
  LastPlayed reversed;
  PlaysPage backwards = page;
  std::vector<PlaysPage::Play> flipped(page.plays.rbegin(), page.plays.rend());
  backwards.plays = flipped;
  addPlays(backwards, reversed);
  check(reversed == latest, "the order plays come in doesn't matter");
}

void testApply() {
  LastPlayed latest = {{1, 20261117}, {2, 20250301}};
  std::vector<Game> games = {game(1, 20200101), game(2), game(3, 20240101)};
  applyLastPlayed(games, latest, false);
  check(games[0].lastPlayed == 20261117 && games[1].lastPlayed == 20250301 && games[2].lastPlayed == 20240101,
        "part read: seen games take their date, the rest keep theirs");
  applyLastPlayed(games, latest, true);
  check(games[2].lastPlayed == 0, "all read: a game with no plays has no date");

  std::vector<Game> old = {game(5, 20260101), game(6, 0), game(7, 20250505)};
  std::vector<Game> fresh = {game(7), game(5), game(8)};
  carryLastPlayed(fresh, old);
  check(fresh[0].lastPlayed == 20250505 && fresh[1].lastPlayed == 20260101 && fresh[2].lastPlayed == 0,
        "a new sync keeps the old dates, matched by id, until the plays are read");
}

void testDates() {
  check(parseDate("2026-11-17") == 20261117, "a date");
  check(parseDate("1999-01-31") == 19990131, "another");
  check(parseDate("") == 0 && parseDate("0000-00-00") == 0, "none, and BGG's empty date");
  check(parseDate("2026-13-01") == 0 && parseDate("2026-11-32") == 0, "no month 13 or day 32");
  check(parseDate("2026/11/17") == 0 && parseDate("2026-1-17") == 0 && parseDate("20x6-11-17") == 0, "wrong shapes");
  check(playsPages(0) == 0 && playsPages(1) == 1 && playsPages(100) == 1 && playsPages(101) == 2, "pages of 100");
}

void testAnswers() {
  PlaysPage page;
  page.total = 9;
  check(parse("<?xml version=\"1.0\"?><plays username=\"x\" total=\"0\" page=\"1\" termsofuse=\"t\"></plays>", page) == PlaysResult::Ok &&
            page.total == 0 && page.plays.empty(),
        "a user with no plays");
  check(parse("<plays username=\"x\" total=\"0\" page=\"1\" />", page) == PlaysResult::Ok && page.plays.empty(), "the same, self-closing");
  page.total = 9;
  check(parse("<?xml version=\"1.0\"?><div class='messagebox error'>\n<error>Invalid object or user</error></div>", page) ==
                PlaysResult::BgError &&
            page.total == 9,
        "BGG's error, leaving the page alone");
  check(parse("<message>accepted</message>", page) == PlaysResult::NotPlays, "not a plays answer");
  const std::string xml = readSample();
  check(parse(xml.substr(0, xml.size() / 2), page) == PlaysResult::Incomplete, "half a page is incomplete");
  check(parse("<plays total=\"2\"><play id=\"1\" date=\"2026-01-01\"><item objectid=\"5\"/></play><play id=\"2\" date=\"2026-01-02\">",
              page) == PlaysResult::Incomplete,
        "a page cut off inside a play");
}

}  // namespace

int main() {
  testSample();
  testApply();
  testDates();
  testAnswers();
  if (failures) {
    std::printf("pocketbgg plays: %d failed\n", failures);
    return 1;
  }
  std::printf("pocketbgg plays: all passed\n");
  return 0;
}
