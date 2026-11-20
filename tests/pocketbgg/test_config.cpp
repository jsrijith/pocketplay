// PocketBGG's config file, URL encoding and the user id in BGG's answer, with no card or network.
// The token here is made up.
#include <cstdio>
#include <cstring>
#include <string>

#include "pocketbgg_config.h"

using namespace pocketbgg;

namespace {

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    failures++;
  }
}

ConfigError parse(const std::string& text, Config& out) { return parseConfig(text.data(), text.size(), out); }

bool parsesTo(const std::string& text, const char* username, const char* token) {
  Config c;
  return parse(text, c) == ConfigError::None && c.username == username && c.token == token;
}

void testConfig() {
  check(parsesTo("username=alice\ntoken=test-token-123\n", "alice", "test-token-123"), "two plain lines");
  check(parsesTo("username=alice\r\ntoken=test-token-123\r\n", "alice", "test-token-123"), "Windows line endings");
  check(parsesTo("\xEF\xBB\xBFusername=alice\ntoken=t1", "alice", "t1"), "a byte-order mark, and no newline at the end");
  check(parsesTo("  username = alice  \n\ttoken =\tt1 \n", "alice", "t1"), "spaces and tabs around keys and values");
  check(parsesTo("# my BGG login\n\nUserName=alice\nTOKEN=t1\n", "alice", "t1"), "comments, blank lines, keys in any case");
  check(parsesTo("username=alice\ncolour=blue\ntoken=t1\nnonsense line\n", "alice", "t1"), "unknown keys and lines are ignored");
  check(parsesTo("username=old\nusername=alice\ntoken=t1\n", "alice", "t1"), "the last value wins");
  check(parsesTo("username=Board Game Fan\ntoken=t1\n", "Board Game Fan", "t1"), "a username with spaces");
  check(parsesTo("username=alice\ntoken=abc=def==\n", "alice", "abc=def=="), "a token holding = keeps everything after the first");

  Config kept;
  kept.username = "before";
  check(parse("", kept) == ConfigError::NoUsername && kept.username == "before", "an empty file, leaving the config alone");
  check(parse("token=t1\n", kept) == ConfigError::NoUsername, "no username");
  check(parse("username=\ntoken=t1\n", kept) == ConfigError::NoUsername, "an empty username");
  check(parse("username=alice\n", kept) == ConfigError::NoToken, "no token");
  check(parse("username=alice\ntoken=   \n", kept) == ConfigError::NoToken, "a token of spaces");
  check(parse("username=alice\ntoken=abc def\n", kept) == ConfigError::BadToken, "a space inside the token");
  check(parse("username=alice\ntoken=abc\x01\n", kept) == ConfigError::BadToken, "a control byte in the token");
  check(parse("username=al\xC3\xAD" "ce\ntoken=t1\n", kept) == ConfigError::BadUsername, "a username past ASCII");
  check(parse("username=alice\ntoken=" + std::string(kMaxToken + 1, 'x') + "\n", kept) == ConfigError::BadToken, "a token too long");
  check(parse("username=" + std::string(kMaxUsername + 1, 'a') + "\ntoken=t1\n", kept) == ConfigError::BadUsername, "a username too long");
  check(kept.username == "before" && kept.token.empty(), "every refused file leaves the config alone");

  for (int e = 0; e <= static_cast<int>(ConfigError::BadToken); e++) {
    const char* text = configErrorText(static_cast<ConfigError>(e));
    check(e == 0 || (text[0] != '\0' && std::strlen(text) < 48), "each error has a short message");
  }
}

void testUrlEncode() {
  check(urlEncode("alice_99") == "alice_99", "letters, digits and _ stay");
  check(urlEncode("a-b.c~d") == "a-b.c~d", "- . ~ stay");
  check(urlEncode("Board Game Fan") == "Board%20Game%20Fan", "a space becomes %20");
  check(urlEncode("a&b=c?d#e") == "a%26b%3Dc%3Fd%23e", "characters that would break the query are escaped");
  check(urlEncode("") == "", "nothing stays nothing");
}

bool userId(const std::string& xml, uint32_t& id) { return parseUserId(xml.data(), xml.size(), id); }

void testUserId() {
  uint32_t id = 0;
  check(userId("<?xml version=\"1.0\"?>\n<user id=\"1234567\" name=\"alice\" termsofuse=\"x\">\n<firstname value=\"\"/></user>", id) &&
            id == 1234567,
        "a known user's id");
  id = 9;
  check(!userId("<user id=\"\" name=\"nobody\" termsofuse=\"x\"></user>", id) && id == 9, "an unknown user has an empty id");
  check(!userId("<error><message>Unauthorized</message></error>", id), "an error answer is no user");
  check(!userId("<user name=\"alice\"><thing id=\"5\"/></user>", id), "an id outside the user tag doesn't count");
  check(!userId("<user id=\"99999999999\" name=\"x\">", id), "an id too big for 32 bits");
  check(!userId("<user id=\"12", id), "a cut-off answer");
  check(!userId("", id), "no answer");
}

}  // namespace

int main() {
  testConfig();
  testUrlEncode();
  testUserId();
  if (failures) {
    std::printf("pocketbgg config: %d failed\n", failures);
    return 1;
  }
  std::printf("pocketbgg config: all passed\n");
  return 0;
}
