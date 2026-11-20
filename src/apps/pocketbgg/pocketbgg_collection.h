#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "pocketbgg_list.h"

// Reads BGG's collection answer, /xmlapi2/collection, into games. No screen, card or network,
// so it's tested off the device with a real answer cut down to five games.
namespace pocketbgg {

enum class ParseResult {
  Ok,
  BgError,        // BGG answered <errors>, such as "Invalid username specified"
  NotCollection,  // no <items>: not a collection answer at all
  Incomplete,     // <items> with no </items>: the answer was cut off
};

struct Collection {
  std::vector<Game> games;
  uint32_t totalItems = 0;  // what BGG says it sent, to check against games.size()
  std::string error;        // BGG's message, for BgError
};

// Fills `out` for Ok, and only `out.error` for BgError. A game with no name or id is skipped
// rather than failing the whole answer, and the games come back sorted (sortGames()).
ParseResult parseCollection(const char* xml, size_t size, Collection& out);

// XML text as PocketPlay can draw it: entities decoded (&amp; &lt; &gt; &quot; &apos;, &#NNN;
// and &#xHH;), then anything past ASCII turned into its nearest ASCII (é to e, – to -, “ to ")
// or ? when there's none. Leading and trailing white space go.
std::string toAscii(const char* text, size_t size);

}  // namespace pocketbgg
