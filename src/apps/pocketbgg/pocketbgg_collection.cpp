#include "pocketbgg_collection.h"

#include <cstdlib>
#include <cstring>

namespace pocketbgg {

namespace {

// The nearest ASCII for the characters past it that board game names use. A code point not
// listed becomes ?, so it shows as something rather than as an outlined box.
struct Ascii {
  uint32_t codePoint;
  const char* text;
};
constexpr Ascii kAscii[] = {
    {0xA0, " "},    {0xA9, "(C)"},  {0xAE, "(R)"},  {0xB0, " deg"}, {0xB2, "2"},    {0xB3, "3"},    {0xB7, "-"},
    {0xBC, "1/4"},  {0xBD, "1/2"},  {0xBE, "3/4"},  {0xC0, "A"},    {0xC1, "A"},    {0xC2, "A"},    {0xC3, "A"},
    {0xC4, "A"},    {0xC5, "A"},    {0xC6, "AE"},   {0xC7, "C"},    {0xC8, "E"},    {0xC9, "E"},    {0xCA, "E"},
    {0xCB, "E"},    {0xCC, "I"},    {0xCD, "I"},    {0xCE, "I"},    {0xCF, "I"},    {0xD0, "D"},    {0xD1, "N"},
    {0xD2, "O"},    {0xD3, "O"},    {0xD4, "O"},    {0xD5, "O"},    {0xD6, "O"},    {0xD7, "x"},    {0xD8, "O"},
    {0xD9, "U"},    {0xDA, "U"},    {0xDB, "U"},    {0xDC, "U"},    {0xDD, "Y"},    {0xDE, "Th"},   {0xDF, "ss"},
    {0xE0, "a"},    {0xE1, "a"},    {0xE2, "a"},    {0xE3, "a"},    {0xE4, "a"},    {0xE5, "a"},    {0xE6, "ae"},
    {0xE7, "c"},    {0xE8, "e"},    {0xE9, "e"},    {0xEA, "e"},    {0xEB, "e"},    {0xEC, "i"},    {0xED, "i"},
    {0xEE, "i"},    {0xEF, "i"},    {0xF0, "d"},    {0xF1, "n"},    {0xF2, "o"},    {0xF3, "o"},    {0xF4, "o"},
    {0xF5, "o"},    {0xF6, "o"},    {0xF7, "/"},    {0xF8, "o"},    {0xF9, "u"},    {0xFA, "u"},    {0xFB, "u"},
    {0xFC, "u"},    {0xFD, "y"},    {0xFE, "th"},   {0xFF, "y"},    {0x100, "A"},   {0x101, "a"},   {0x102, "A"},
    {0x103, "a"},   {0x104, "A"},   {0x105, "a"},   {0x106, "C"},   {0x107, "c"},   {0x10C, "C"},   {0x10D, "c"},
    {0x10E, "D"},   {0x10F, "d"},   {0x110, "D"},   {0x111, "d"},   {0x112, "E"},   {0x113, "e"},   {0x116, "E"},
    {0x117, "e"},   {0x118, "E"},   {0x119, "e"},   {0x11A, "E"},   {0x11B, "e"},   {0x11E, "G"},   {0x11F, "g"},
    {0x12A, "I"},   {0x12B, "i"},   {0x130, "I"},   {0x131, "i"},   {0x141, "L"},   {0x142, "l"},   {0x143, "N"},
    {0x144, "n"},   {0x147, "N"},   {0x148, "n"},   {0x14C, "O"},   {0x14D, "o"},   {0x150, "O"},   {0x151, "o"},
    {0x152, "OE"},  {0x153, "oe"},  {0x158, "R"},   {0x159, "r"},   {0x15A, "S"},   {0x15B, "s"},   {0x15E, "S"},
    {0x15F, "s"},   {0x160, "S"},   {0x161, "s"},   {0x162, "T"},   {0x163, "t"},   {0x164, "T"},   {0x165, "t"},
    {0x16A, "U"},   {0x16B, "u"},   {0x16E, "U"},   {0x16F, "u"},   {0x170, "U"},   {0x171, "u"},   {0x178, "Y"},
    {0x179, "Z"},   {0x17A, "z"},   {0x17B, "Z"},   {0x17C, "z"},   {0x17D, "Z"},   {0x17E, "z"},   {0x2010, "-"},
    {0x2011, "-"},  {0x2012, "-"},  {0x2013, "-"},  {0x2014, "-"},  {0x2015, "-"},  {0x2018, "'"},  {0x2019, "'"},
    {0x201A, "'"},  {0x201C, "\""}, {0x201D, "\""}, {0x201E, "\""}, {0x2022, "-"},  {0x2026, "..."}, {0x2032, "'"},
    {0x2033, "\""}, {0x2122, "(TM)"}, {0x2212, "-"},
};

void appendAscii(std::string& out, uint32_t codePoint) {
  if (codePoint < 0x80) {
    // Control characters other than white space don't belong in a name.
    if (codePoint >= 0x20 || codePoint == '\t' || codePoint == '\n' || codePoint == '\r') out += static_cast<char>(codePoint);
    return;
  }
  for (const Ascii& a : kAscii) {
    if (a.codePoint == codePoint) {
      out += a.text;
      return;
    }
  }
  out += '?';
}

// One UTF-8 character from s[i], moving i past it. A broken sequence counts as one byte, ?.
uint32_t nextCodePoint(const char* s, size_t size, size_t& i) {
  const uint8_t c = static_cast<uint8_t>(s[i]);
  int extra = 0;
  uint32_t cp = 0;
  if (c < 0x80) {
    i++;
    return c;
  } else if ((c & 0xE0) == 0xC0) {
    extra = 1;
    cp = c & 0x1F;
  } else if ((c & 0xF0) == 0xE0) {
    extra = 2;
    cp = c & 0x0F;
  } else if ((c & 0xF8) == 0xF0) {
    extra = 3;
    cp = c & 0x07;
  } else {
    i++;
    return 0xFFFD;
  }
  // The bytes after the first must all be there: a name cut off mid-character is broken.
  if (i + extra >= size) {
    i++;
    return 0xFFFD;
  }
  for (int k = 1; k <= extra; k++) {
    const uint8_t next = static_cast<uint8_t>(s[i + k]);
    if ((next & 0xC0) != 0x80) {
      i++;
      return 0xFFFD;
    }
    cp = (cp << 6) | (next & 0x3F);
  }
  i += 1 + extra;
  return cp;
}

// An entity starting at s[i] ('&'), moving i past it. Not one we know: the & stays as it is.
uint32_t entity(const char* s, size_t size, size_t& i) {
  const size_t semi = [&] {
    for (size_t k = i + 1; k < size && k < i + 12; k++) {
      if (s[k] == ';') return k;
    }
    return size;
  }();
  if (semi == size) {
    i++;
    return '&';
  }
  const std::string name(s + i + 1, semi - i - 1);
  uint32_t cp = 0;
  if (name == "amp") cp = '&';
  else if (name == "lt") cp = '<';
  else if (name == "gt") cp = '>';
  else if (name == "quot") cp = '"';
  else if (name == "apos") cp = '\'';
  else if (name.size() > 1 && name[0] == '#') {
    const bool hex = name[1] == 'x' || name[1] == 'X';
    const char* digits = name.c_str() + (hex ? 2 : 1);
    char* end = nullptr;
    const unsigned long value = std::strtoul(digits, &end, hex ? 16 : 10);
    if (end == digits || *end != '\0' || value > 0x10FFFF) {
      i++;
      return '&';
    }
    cp = static_cast<uint32_t>(value);
  } else {
    i++;
    return '&';
  }
  i = semi + 1;
  return cp;
}

// The value of attribute `key` in a tag's text, such as objectid in <item objectid="1" ...>.
// The space before the key keeps "id" from matching inside "objectid".
bool attribute(const std::string& tag, const char* key, std::string& value) {
  const std::string needle = std::string(" ") + key + "=\"";
  size_t at = tag.find(needle);
  if (at == std::string::npos) {
    // Tabs separate BGG's <stats> attributes.
    const std::string tabbed = std::string("\t") + key + "=\"";
    at = tag.find(tabbed);
    if (at == std::string::npos) return false;
  }
  const size_t start = at + needle.size();
  const size_t end = tag.find('"', start);
  if (end == std::string::npos) return false;
  value = tag.substr(start, end - start);
  return true;
}

long attributeNumber(const std::string& tag, const char* key, long fallback) {
  std::string value;
  if (!attribute(tag, key, value) || value.empty()) return fallback;
  char* end = nullptr;
  const long n = std::strtol(value.c_str(), &end, 10);
  return *end == '\0' ? n : fallback;
}

// The opening tag <name ...>, from the < to the >, inside [from, to). Empty if none. `name` must
// be followed by a space, a tab or >, so <name> doesn't match <numplays>.
std::string openingTag(const std::string& xml, size_t from, size_t to, const char* name, size_t* after = nullptr) {
  const std::string start = std::string("<") + name;
  size_t at = from;
  while ((at = xml.find(start, at)) != std::string::npos && at < to) {
    const char next = at + start.size() < xml.size() ? xml[at + start.size()] : '\0';
    if (next == ' ' || next == '\t' || next == '>' || next == '/' || next == '\n') {
      const size_t close = xml.find('>', at);
      if (close == std::string::npos || close >= to) return "";
      if (after) *after = close + 1;
      return xml.substr(at, close + 1 - at);
    }
    at += start.size();
  }
  return "";
}

// The text inside <name ...>text</name>, within [from, to).
bool elementText(const std::string& xml, size_t from, size_t to, const char* name, std::string& text, std::string* tag = nullptr) {
  size_t after = 0;
  const std::string open = openingTag(xml, from, to, name, &after);
  if (open.empty()) return false;
  if (tag) *tag = open;
  if (open[open.size() - 2] == '/') {
    text.clear();
    return true;
  }
  const size_t close = xml.find(std::string("</") + name + ">", after);
  if (close == std::string::npos || close > to) return false;
  text = xml.substr(after, close - after);
  return true;
}

long number(const std::string& text, long fallback) {
  const std::string t = toAscii(text.data(), text.size());
  if (t.empty()) return fallback;
  char* end = nullptr;
  const long n = std::strtol(t.c_str(), &end, 10);
  return *end == '\0' ? n : fallback;
}

uint16_t clamp16(long n) { return static_cast<uint16_t>(n < 0 ? 0 : n > 0xFFFF ? 0xFFFF : n); }
uint8_t clamp8(long n) { return static_cast<uint8_t>(n < 0 ? 0 : n > 0xFF ? 0xFF : n); }

bool readGame(const std::string& xml, size_t from, size_t to, Game& game) {
  const std::string item = openingTag(xml, from, to, "item");
  const long id = attributeNumber(item, "objectid", 0);
  if (id <= 0) return false;
  game.id = static_cast<uint32_t>(id);
  game.collId = static_cast<uint32_t>(attributeNumber(item, "collid", 0));

  std::string text, tag;
  if (!elementText(xml, from, to, "name", text, &tag)) return false;
  game.name = toAscii(text.data(), text.size());
  if (game.name.empty()) return false;
  // sortindex counts from 1: 5 for "The Crew" means start at the fifth character.
  const long sortIndex = attributeNumber(tag, "sortindex", 1);
  game.sortSkip = clamp8(sortIndex - 1);

  if (elementText(xml, from, to, "yearpublished", text)) {
    const long year = number(text, kNoYear);
    game.year = year < -32767 || year > 32767 ? kNoYear : static_cast<int16_t>(year);
  }
  if (elementText(xml, from, to, "numplays", text)) game.plays = clamp16(number(text, 0));
  if (elementText(xml, from, to, "thumbnail", text)) game.thumbnail = toAscii(text.data(), text.size());

  const std::string stats = openingTag(xml, from, to, "stats");
  if (!stats.empty()) {
    game.minPlayers = clamp8(attributeNumber(stats, "minplayers", 0));
    game.maxPlayers = clamp8(attributeNumber(stats, "maxplayers", 0));
    game.playTime = clamp16(attributeNumber(stats, "playingtime", 0));
  }

  const std::string status = openingTag(xml, from, to, "status");
  if (!status.empty()) {
    static const struct {
      const char* key;
      uint16_t flag;
    } FLAGS[] = {
        {"own", kOwn},           {"prevowned", kPrevOwned}, {"fortrade", kForTrade}, {"want", kWant},
        {"wanttoplay", kWantToPlay}, {"wanttobuy", kWantToBuy}, {"wishlist", kWishlist}, {"preordered", kPreordered},
    };
    for (const auto& f : FLAGS) {
      if (attributeNumber(status, f.key, 0) == 1) game.status |= f.flag;
    }
    std::string modified;
    if (attribute(status, "lastmodified", modified)) game.lastModified = modified;
  }
  return true;
}

}  // namespace

std::string toAscii(const char* text, size_t size) {
  std::string out;
  out.reserve(size);
  size_t i = 0;
  while (i < size) {
    if (text[i] == '&') {
      appendAscii(out, entity(text, size, i));
    } else {
      appendAscii(out, nextCodePoint(text, size, i));
    }
  }
  const size_t first = out.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return "";
  const size_t last = out.find_last_not_of(" \t\r\n");
  return out.substr(first, last - first + 1);
}

ParseResult parseCollection(const char* data, size_t size, Collection& out) {
  const std::string xml(data, size);
  if (xml.find("<errors") != std::string::npos) {
    std::string message;
    out.error = elementText(xml, 0, xml.size(), "message", message) ? toAscii(message.data(), message.size()) : "";
    return ParseResult::BgError;
  }
  size_t itemsAfter = 0;
  const std::string items = openingTag(xml, 0, xml.size(), "items", &itemsAfter);
  if (items.empty()) return ParseResult::NotCollection;
  const size_t end = xml.find("</items>", itemsAfter);
  if (end == std::string::npos) return ParseResult::Incomplete;

  Collection result;
  result.totalItems = static_cast<uint32_t>(attributeNumber(items, "totalitems", 0));
  size_t at = itemsAfter;
  while (true) {
    const size_t start = xml.find("<item ", at);
    if (start == std::string::npos || start >= end) break;
    const size_t stop = xml.find("</item>", start);
    if (stop == std::string::npos || stop > end) return ParseResult::Incomplete;
    Game game;
    if (readGame(xml, start, stop, game)) result.games.push_back(std::move(game));
    at = stop + 7;
  }
  sortGames(result.games);
  out = std::move(result);
  return ParseResult::Ok;
}

}  // namespace pocketbgg
