// PocketBGG's collection parser, on a real BGG answer cut down to five games
// (sample_collection.xml) and made-up ones for the cases it doesn't have.
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include "pocketbgg_collection.h"

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
  std::ifstream file("sample_collection.xml", std::ios::binary);
  std::stringstream text;
  text << file.rdbuf();
  return text.str();
}

ParseResult parse(const std::string& xml, Collection& out) { return parseCollection(xml.data(), xml.size(), out); }

std::string ascii(const std::string& text) { return toAscii(text.data(), text.size()); }

const Game* find(const Collection& c, uint32_t id) {
  for (const Game& g : c.games) {
    if (g.id == id) return &g;
  }
  return nullptr;
}

void testSample() {
  const std::string xml = readSample();
  check(!xml.empty(), "the sample file is there");
  Collection c;
  check(parse(xml, c) == ParseResult::Ok, "the real answer parses");
  check(c.totalItems == 5 && c.games.size() == 5, "five games, as totalitems says");

  check(c.games.size() == 5 && c.games[0].name == "Math Dice Jr." && c.games[1].name == "Nine Men's Morris" &&
            c.games[2].name == "Pandemic: Hot Zone - North America" && c.games[3].name == "Pokemon Trading Card Game: Battle Academy" &&
            c.games[4].name == "Silver & Gold",
        "sorted by name, with &#039; &amp; - and e in plain ASCII");

  const Game* pokemon = find(c, 310953);
  check(pokemon && pokemon->collId == 98065238 && pokemon->year == 2020 && pokemon->plays == 33, "Pokemon's ids, year and plays");
  check(pokemon && pokemon->status == kOwn && pokemon->lastModified.size() == 19, "owned, with a last-modified time");
  check(pokemon && pokemon->thumbnail.rfind("https://cf.geekdo-images.com/", 0) == 0 && pokemon->thumbnail.find(".jpg") != std::string::npos,
        "a thumbnail URL");
  const Game* morris = find(c, 3886);
  check(morris && morris->year == -1400, "Nine Men's Morris is from 1400 BC");
  const Game* pandemic = find(c, 301919);
  check(pandemic && pandemic->thumbnail.find(".png") != std::string::npos, "Pandemic's thumbnail is a PNG");
  const Game* dice = find(c, 102750);
  check(dice && dice->year == kNoYear && dice->plays == 2, "Math Dice Jr. has no year");
  check(dice && dice->minPlayers == 2 && dice->maxPlayers == 0 && dice->playTime == 10, "and no maximum player count");
  check(dice && dice->lastModified == "2020-07-26 01:25:08", "its last-modified time as BGG sent it");
  for (const Game& g : c.games) {
    bool plain = true;
    for (char ch : g.name) plain = plain && ch >= 0x20 && ch < 0x7F;
    check(plain, "every name is plain ASCII");
  }

  Collection cut;
  check(parse(xml.substr(0, xml.size() / 2), cut) == ParseResult::Incomplete, "half an answer is incomplete");
  check(parse(xml.substr(0, xml.size() - 10), cut) == ParseResult::Incomplete, "an answer missing its end is incomplete");
}

void testAnswers() {
  Collection c;
  c.totalItems = 99;
  check(parse("<?xml version=\"1.0\"?>\n<errors>\n<error><message>Invalid username specified</message></error></errors>", c) ==
                ParseResult::BgError &&
            c.error == "Invalid username specified" && c.totalItems == 99,
        "BGG's error message, leaving the collection alone");
  check(parse("<?xml version=\"1.0\"?>\n<message>Your request for this collection has been accepted.</message>", c) ==
            ParseResult::NotCollection,
        "the 202 'accepted' message isn't a collection");
  check(parse("", c) == ParseResult::NotCollection, "nothing isn't a collection");
  check(parse("<items totalitems=\"0\" termsofuse=\"x\">\n</items>", c) == ParseResult::Ok && c.games.empty(), "an empty collection");

  const std::string made =
      "<items totalitems=\"3\">"
      "<item objecttype=\"thing\" objectid=\"7\" subtype=\"boardgame\" collid=\"70\"><name sortindex=\"5\">The Crew</name>"
      "<status own=\"0\" prevowned=\"1\" fortrade=\"0\" want=\"0\" wanttoplay=\"0\" wanttobuy=\"0\" wishlist=\"1\" preordered=\"0\" "
      "lastmodified=\"2024-01-02 03:04:05\" /><numplays>0</numplays></item>"
      "<item objecttype=\"thing\" objectid=\"8\" collid=\"80\"><name sortindex=\"1\"></name></item>"
      "<item objecttype=\"thing\" objectid=\"9\" collid=\"90\"><name sortindex=\"1\">Azul</name>"
      "<yearpublished>abc</yearpublished><numplays>99999</numplays></item>"
      "</items>";
  check(parse(made, c) == ParseResult::Ok && c.games.size() == 2, "a game with no name is skipped");
  check(c.games.size() == 2 && c.games[0].name == "Azul" && c.games[1].name == "The Crew", "The Crew sorts under C, after Azul");
  check(c.games.size() == 2 && c.games[1].status == (kPrevOwned | kWishlist) && c.games[1].sortSkip == 4, "status flags and sortindex");
  check(c.games.size() == 2 && c.games[0].year == kNoYear && c.games[0].plays == 65535, "a bad year is no year, and plays stop at 65535");
}

void testAscii() {
  check(ascii("SH\xC5\x8C" "BU") == "SHOBU", "O with a macron");
  check(ascii("Pok\xC3\xA9mon") == "Pokemon", "e with an accent");
  check(ascii("Hot Zone \xE2\x80\x93 North America") == "Hot Zone - North America", "an en dash");
  check(ascii("\xE2\x80\x9CQuoted\xE2\x80\x9D \xE2\x80\x99s") == "\"Quoted\" 's", "curly quotes");
  check(ascii("Stra\xC3\x9F" "e \xC3\x86on") == "Strasse AEon", "letters that become two");
  check(ascii("&amp;&lt;&gt;&quot;&apos;&#039;&#x41;&#233;") == "&<>\"''Ae", "named, decimal and hex entities");
  check(ascii("R&D &unknown; &#xZZ; &") == "R&D &unknown; &#xZZ; &", "anything not an entity stays as it is");
  check(ascii("\xE4\xB8\xAD\xE6\x96\x87") == "??", "characters with no ASCII become ?");
  check(ascii("bad \xC3") == "bad ?", "a character cut off at the end");
  check(ascii("bad \xC3(") == "bad ?(", "a broken sequence");
  check(ascii("  \t spaced out \n ") == "spaced out", "white space at the ends goes");
  check(ascii("a\x01" "b") == "ab", "control characters go");
}

}  // namespace

int main() {
  testSample();
  testAnswers();
  testAscii();
  if (failures) {
    std::printf("pocketbgg collection: %d failed\n", failures);
    return 1;
  }
  std::printf("pocketbgg collection: all passed\n");
  return 0;
}
