#include "pocketbgg_xml.h"

#include <cstdlib>

namespace pocketbgg {
namespace xml {

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
std::string openingTag(const std::string& xml, size_t from, size_t to, const char* name, size_t* after) {
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
bool elementText(const std::string& xml, size_t from, size_t to, const char* name, std::string& text, std::string* tag) {
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

}  // namespace xml
}  // namespace pocketbgg
