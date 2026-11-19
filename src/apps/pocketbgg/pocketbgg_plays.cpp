#include "pocketbgg_plays.h"

#include <unordered_map>

#include "pocketbgg_xml.h"

namespace pocketbgg {

uint32_t parseDate(const std::string& text) {
  if (text.size() != 10 || text[4] != '-' || text[7] != '-') return 0;
  uint32_t parts[3] = {0, 0, 0};
  const size_t starts[3] = {0, 5, 8};
  const size_t lengths[3] = {4, 2, 2};
  for (int p = 0; p < 3; p++) {
    for (size_t i = 0; i < lengths[p]; i++) {
      const char c = text[starts[p] + i];
      if (c < '0' || c > '9') return 0;
      parts[p] = parts[p] * 10 + static_cast<uint32_t>(c - '0');
    }
  }
  const uint32_t year = parts[0], month = parts[1], day = parts[2];
  if (year < 1900 || year > 2200 || month < 1 || month > 12 || day < 1 || day > 31) return 0;
  return year * 10000 + month * 100 + day;
}

int playsPages(uint32_t total) { return static_cast<int>((total + kPlaysPerPage - 1) / kPlaysPerPage); }

PlaysResult parsePlaysPage(const char* data, size_t size, PlaysPage& out) {
  const std::string text(data, size);
  if (text.find("<errors") != std::string::npos || text.find("<error>") != std::string::npos) {
    std::string message;
    out.error = xml::elementText(text, 0, text.size(), "message", message) ? message : "";
    return PlaysResult::BgError;
  }
  size_t after = 0;
  const std::string plays = xml::openingTag(text, 0, text.size(), "plays", &after);
  if (plays.empty()) return PlaysResult::NotPlays;
  // A user with no plays gets <plays ... /> or <plays ...></plays>.
  const bool selfClosing = plays.size() >= 2 && plays[plays.size() - 2] == '/';
  const size_t end = selfClosing ? after : text.find("</plays>", after);
  if (end == std::string::npos) return PlaysResult::Incomplete;

  PlaysPage page;
  page.total = static_cast<uint32_t>(xml::attributeNumber(plays, "total", 0));
  size_t at = after;
  while (true) {
    size_t playAfter = 0;
    const std::string play = xml::openingTag(text, at, end, "play", &playAfter);
    if (play.empty()) break;
    const size_t stop = text.find("</play>", playAfter);
    if (stop == std::string::npos || stop > end) return PlaysResult::Incomplete;
    std::string dateText;
    xml::attribute(play, "date", dateText);
    const uint32_t date = parseDate(dateText);
    const std::string item = xml::openingTag(text, playAfter, stop, "item");
    const long gameId = xml::attributeNumber(item, "objectid", 0);
    if (date != 0 && gameId > 0) page.plays.push_back({static_cast<uint32_t>(gameId), date});
    at = stop + 7;
  }
  out = std::move(page);
  return PlaysResult::Ok;
}

void addPlays(const PlaysPage& page, LastPlayed& latest) {
  for (const PlaysPage::Play& play : page.plays) {
    uint32_t& date = latest[play.gameId];
    if (play.date > date) date = play.date;
  }
}

void applyLastPlayed(std::vector<Game>& games, const LastPlayed& latest, bool complete) {
  for (Game& g : games) {
    const auto found = latest.find(g.id);
    if (found != latest.end()) {
      g.lastPlayed = found->second;
    } else if (complete) {
      g.lastPlayed = 0;
    }
  }
}

void carryLastPlayed(std::vector<Game>& fresh, const std::vector<Game>& old) {
  std::unordered_map<uint32_t, uint32_t> dates;
  for (const Game& g : old) {
    if (g.lastPlayed != 0) dates[g.id] = g.lastPlayed;
  }
  for (Game& g : fresh) {
    const auto found = dates.find(g.id);
    if (found != dates.end()) g.lastPlayed = found->second;
  }
}

}  // namespace pocketbgg
