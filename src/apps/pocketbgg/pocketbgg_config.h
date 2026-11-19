#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

// The player's BGG username and API token, from a text file they copy onto the card. No screen or
// card here, so it can be tested off the device: the screen reads the file and hands its bytes in.
namespace pocketbgg {

constexpr const char* kConfigFolder = "/pocketbgg";
constexpr const char* kConfigFile = "config.txt";
constexpr const char* kConfigPath = "/pocketbgg/config.txt";
constexpr size_t kMaxConfigBytes = 1024;
constexpr size_t kMaxUsername = 64;
constexpr size_t kMaxToken = 256;

struct Config {
  std::string username;
  std::string token;
};

enum class ConfigError { None, Missing, Unreadable, NoUsername, NoToken, BadUsername, BadToken };

// Lines of key=value, as Notepad saves them: \r\n or \n, an optional UTF-8 byte-order mark,
// spaces around keys and values, blank lines and # comments. Keys are username and token, in
// any case, and others are ignored; a key given twice keeps its last value. Returns None and
// fills `out`, or the first thing wrong, leaving `out` alone.
ConfigError parseConfig(const char* text, size_t size, Config& out);

// What to show for an error, short enough for the status line.
const char* configErrorText(ConfigError error);

// A username as it can go in a URL's query: letters, digits and -._~ stay, and everything
// else, a space included, becomes %XX.
std::string urlEncode(const std::string& text);

// The id in BGG's answer about a user, <user id="1234" name="...">. BGG answers an unknown
// name with id="", so false means no such user, or an answer that isn't about a user at all.
bool parseUserId(const char* xml, size_t size, uint32_t& id);

// Overwrites a string's bytes before it's dropped, so a token doesn't linger in memory.
void wipe(std::string& secret);

}  // namespace pocketbgg
