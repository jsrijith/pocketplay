#pragma once

#include <cstddef>
#include <string>

// The few pieces of XML scanning BGG's answers need, shared by the collection and plays parsers.
// Not a general XML parser: BGG's answers are regular, so these find tags and attributes by text,
// within a range, and never allocate more than the piece they return.
namespace pocketbgg {
namespace xml {

// The value of attribute `key` in a tag's text, such as objectid in <item objectid="1" ...>.
// A space or tab must come before the key, so "id" doesn't match inside "objectid".
bool attribute(const std::string& tag, const char* key, std::string& value);

// The attribute as a whole number, or `fallback` if it's missing, empty or not a number.
long attributeNumber(const std::string& tag, const char* key, long fallback);

// The opening tag <name ...>, from the < to the >, inside [from, to), or empty if there's none.
// `name` must be followed by a space, tab, newline, / or >, so <name> doesn't match <numplays>.
// `after`, if given, is set to the position just past the tag.
std::string openingTag(const std::string& xml, size_t from, size_t to, const char* name, size_t* after = nullptr);

// The text inside <name ...>text</name> within [from, to), and the opening tag if `tag` is given.
// A self-closing <name ... /> has empty text.
bool elementText(const std::string& xml, size_t from, size_t to, const char* name, std::string& text, std::string* tag = nullptr);

}  // namespace xml
}  // namespace pocketbgg
