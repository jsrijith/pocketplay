#include "pocketbgg_config.h"

#include <cstring>

namespace pocketbgg {

namespace {

bool isSpace(char c) { return c == ' ' || c == '\t'; }

std::string trim(const char* begin, const char* end) {
  while (begin < end && isSpace(*begin)) begin++;
  while (end > begin && isSpace(end[-1])) end--;
  return std::string(begin, end);
}

std::string lower(std::string text) {
  for (char& c : text) {
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  }
  return text;
}

// A username may hold spaces (urlEncode sends them), but no control bytes or anything past
// ASCII, which PocketPlay's font can't draw.
bool validUsername(const std::string& name) {
  if (name.empty() || name.size() > kMaxUsername) return false;
  for (char c : name) {
    if (c < 0x20 || c > 0x7E) return false;
  }
  return true;
}

// A token is one unbroken run of visible ASCII: a space inside means a paste went wrong.
bool validToken(const std::string& token) {
  if (token.empty() || token.size() > kMaxToken) return false;
  for (char c : token) {
    if (c < 0x21 || c > 0x7E) return false;
  }
  return true;
}

}  // namespace

ConfigError parseConfig(const char* text, size_t size, Config& out) {
  const char* p = text;
  const char* end = text + size;
  if (size >= 3 && std::memcmp(p, "\xEF\xBB\xBF", 3) == 0) p += 3;

  bool haveUsername = false, haveToken = false;
  std::string username, token;
  while (p < end) {
    const char* lineEnd = p;
    while (lineEnd < end && *lineEnd != '\n') lineEnd++;
    const char* contentEnd = lineEnd;
    if (contentEnd > p && contentEnd[-1] == '\r') contentEnd--;

    const std::string line = trim(p, contentEnd);
    p = lineEnd < end ? lineEnd + 1 : end;
    if (line.empty() || line[0] == '#') continue;
    const size_t equals = line.find('=');
    if (equals == std::string::npos) continue;

    const std::string key = lower(trim(line.data(), line.data() + equals));
    const std::string value = trim(line.data() + equals + 1, line.data() + line.size());
    if (key == "username") {
      username = value;
      haveUsername = true;
    } else if (key == "token") {
      wipe(token);
      token = value;
      haveToken = true;
    }
  }

  ConfigError error = ConfigError::None;
  if (!haveUsername || username.empty()) error = ConfigError::NoUsername;
  else if (!validUsername(username)) error = ConfigError::BadUsername;
  else if (!haveToken || token.empty()) error = ConfigError::NoToken;
  else if (!validToken(token)) error = ConfigError::BadToken;
  if (error == ConfigError::None) {
    out.username = username;
    wipe(out.token);
    out.token = token;
  }
  wipe(token);
  return error;
}

const char* configErrorText(ConfigError error) {
  switch (error) {
    case ConfigError::None:
      return "";
    case ConfigError::Missing:
      return "NO CONFIG.TXT IN /POCKETBGG";
    case ConfigError::Unreadable:
      return "CONFIG.TXT UNREADABLE OR OVER 1 KB";
    case ConfigError::NoUsername:
      return "CONFIG.TXT HAS NO USERNAME=";
    case ConfigError::NoToken:
      return "CONFIG.TXT HAS NO TOKEN=";
    case ConfigError::BadUsername:
      return "ODD CHARACTER IN THE USERNAME";
    case ConfigError::BadToken:
      return "SPACE OR ODD CHARACTER IN TOKEN";
  }
  return "";
}

std::string urlEncode(const std::string& text) {
  static const char* const HEX = "0123456789ABCDEF";
  std::string out;
  for (unsigned char c : text) {
    const bool plain = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '.' ||
                       c == '_' || c == '~';
    if (plain) {
      out += static_cast<char>(c);
    } else {
      out += '%';
      out += HEX[c >> 4];
      out += HEX[c & 0x0F];
    }
  }
  return out;
}

bool parseUserId(const char* xml, size_t size, uint32_t& id) {
  const std::string text(xml, size);
  const size_t tag = text.find("<user ");
  if (tag == std::string::npos) return false;
  const size_t tagEnd = text.find('>', tag);
  if (tagEnd == std::string::npos) return false;
  const size_t attr = text.find(" id=\"", tag);
  if (attr == std::string::npos || attr > tagEnd) return false;

  uint64_t value = 0;
  size_t i = attr + 5;
  const size_t digitsStart = i;
  while (i < tagEnd && text[i] >= '0' && text[i] <= '9') {
    value = value * 10 + static_cast<uint64_t>(text[i] - '0');
    if (value > 0xFFFFFFFFu) return false;
    i++;
  }
  if (i == digitsStart || i >= tagEnd || text[i] != '"') return false;
  id = static_cast<uint32_t>(value);
  return true;
}

void wipe(std::string& secret) {
  for (char& c : secret) c = '\0';
  secret.clear();
}

}  // namespace pocketbgg
