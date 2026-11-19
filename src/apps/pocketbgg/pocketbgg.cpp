#include "pocketbgg.h"

#include <Arduino.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "core/clock.h"
#include "core/download.h"
#include "core/graphics.h"
#include "core/network.h"
#include "core/storage.h"
#include "core/text.h"
#include "pocketbgg_ca.h"
#include "pocketbgg_collection.h"
#include "pocketbgg_config.h"
#include "pocketbgg_list.h"
#include "pocketbgg_sample.h"
#include "ui/ui.h"

namespace bgg = pocketbgg;

namespace {

// The list: a caption line saying where the data came from and which page this is, then one
// row per game (the name, wrapped, and a line under it with the year and plays), then a status
// line and SYNC along the bottom. The Up and Down buttons turn the pages. The details: the same
// caption, a box where the thumbnail will go, the name large, and the year and plays.
constexpr int MARGIN = 16;
constexpr int GAP = 12;
constexpr int INNER_WIDTH = SCREEN_WIDTH - 2 * MARGIN;

constexpr int CAPTION_TOP = ui::CONTENT_TOP;
constexpr int CAPTION_HEIGHT = 24;
constexpr int LIST_TOP = CAPTION_TOP + CAPTION_HEIGHT + 8;

constexpr Font NAME_FONT = Font::Size20;
constexpr Font META_FONT = Font::Size14;
constexpr int ROW_INSET = 14;  // the text's distance from the row's outline, at the sides
constexpr int ROW_PAD_TOP = 6;
constexpr int ROW_PAD_BOTTOM = 8;
constexpr int NAME_WIDTH = INNER_WIDTH - 2 * ROW_INSET;
constexpr int META_HEIGHT = 26;

// The bottom row holds the actions: SYNC for now, with room beside it for more later, such as
// picking a random game. The status line above it says how the last sync went.
constexpr int BUTTON_TOP = SCREEN_HEIGHT - MARGIN - ui::PILL_HEIGHT;
constexpr int STATUS_HEIGHT = 24;
constexpr int STATUS_TOP = BUTTON_TOP - 6 - STATUS_HEIGHT;
constexpr int LIST_BOTTOM = STATUS_TOP - 6;
constexpr int LIST_ROOM = LIST_BOTTOM - LIST_TOP;

// A sync downloads the whole collection, every status and expansions too, with stats for the
// player counts and play time. It's signed in with the token, over a connection whose
// certificate must lead to BGG's root.
constexpr const char* COLLECTION_URL = "https://boardgamegeek.com/xmlapi2/collection?stats=1&username=";
// BGG first answers 202, "queued", while it gathers a collection, so the app asks again after a
// wait. One retry has been enough so far; six covers a busy day.
constexpr uint32_t RETRY_WAIT_MS = 5000;
constexpr int MAX_ATTEMPTS = 6;
// The answer goes to PSRAM: malloc puts anything over 4 KB there. 143 games came to 192 KB with
// only owned games, so this reserves room for more and stops at 1 MB, about 750 games.
constexpr size_t EXPECTED_ANSWER_BYTES = 256 * 1024;
constexpr size_t MAX_ANSWER_BYTES = 1024 * 1024;
constexpr size_t PROGRESS_STEP_BYTES = 64 * 1024;  // a redraw each time this much more has come

constexpr int THUMB = 216;  // BGG's thumbnails are up to 200 x 150, so this leaves a border
constexpr int THUMB_LEFT = (SCREEN_WIDTH - THUMB) / 2;
constexpr int THUMB_TOP = LIST_TOP;
constexpr int DETAIL_NAME_TOP = THUMB_TOP + THUMB + 2 * GAP;

int rowHeight(const bgg::Game& game) {
  return ROW_PAD_TOP + wrappedHeight(NAME_FONT, NAME_WIDTH, game.name.c_str()) + META_HEIGHT + ROW_PAD_BOTTOM;
}

class PocketBgg final : public App {
 public:
  const char* name() const override { return "POCKETBGG"; }

  void open() override {
    // So the player can see where config.txt goes when the card is in a PC. Only the folder:
    // the file is theirs to write.
    if (storage::ready()) storage::ensureFolder(bgg::kConfigFolder);
    if (!heights_.empty()) return;
    showGames(bgg::sampleGames());
  }

  TapResult onTap(int x, int y) override {
    if (ui::isOnBack(x, y)) {
      if (selected_ < 0) {
        // A download has to stop before the radio can go off, so the first BACK asks it to,
        // and the app stays until it has.
        if (sync_ == Sync::Downloading || sync_ == Sync::Stopping) {
          if (sync_ == Sync::Stopping) return TapResult::Nothing;
          download::requestCancel();
          sync_ = Sync::Stopping;
          return TapResult::Redraw;
        }
        stopSync();
        return TapResult::Leave;
      }
      selected_ = -1;
      return TapResult::RedrawFull;
    }
    if (selected_ >= 0) return TapResult::Nothing;

    if (ui::isInBox(x, y, MARGIN, BUTTON_TOP, INNER_WIDTH, ui::PILL_HEIGHT)) return startSync();

    for (int row = firstRow(); row < endRow(); row++) {
      if (ui::isInBox(x, y, MARGIN, rowTop(row), INNER_WIDTH, heights_[row])) {
        selected_ = row;
        return TapResult::RedrawFull;
      }
    }
    return TapResult::Nothing;
  }

  // Up and Down turn the list's pages, and do nothing past the first or last. On a game's
  // details they do nothing yet.
  TapResult onUpPressed() override { return selected_ < 0 ? turnPage(-1) : TapResult::Nothing; }
  TapResult onDownPressed() override { return selected_ < 0 ? turnPage(1) : TapResult::Nothing; }

  void draw(uint8_t* frame) override {
    ui::drawBand(frame, name(), nullptr, true);
    if (selected_ < 0) {
      drawList(frame);
    } else {
      drawDetails(frame, games_[selected_]);
    }
  }

  // Watches the download, which runs in its own task, waits out BGG's 202s, and reads the
  // collection once it has come. Called every loop while the app is on screen; true asks for a
  // redraw.
  bool update() override {
    if (sync_ == Sync::Waiting) {
      if (millis() - waitStartMs_ < RETRY_WAIT_MS) return false;
      startAttempt();
      return true;
    }
    if (sync_ != Sync::Downloading && sync_ != Sync::Stopping) return false;
    if (download::state() == download::State::Running) {
      // The task counts what has come, as answer_ itself belongs to it until it ends.
      const size_t received = received_.load();
      if (received < shownBytes_ + PROGRESS_STEP_BYTES) return false;
      shownBytes_ = received;
      return true;
    }

    if (sync_ == Sync::Stopping) {
      finishSync(Sync::Never);
      return true;
    }
    status_ = download::httpStatus();
    Serial.printf("PocketBGG: attempt %d, HTTP %d, %u bytes\n", attempt_, status_, static_cast<unsigned>(answer_.size()));
    if (status_ == 202) {
      if (attempt_ >= MAX_ATTEMPTS) {
        finishSync(Sync::StillQueued);
      } else {
        sync_ = Sync::Waiting;
        waitStartMs_ = millis();
      }
      return true;
    }
    if (status_ == 401 || status_ == 403) {
      finishSync(Sync::TokenRefused);
    } else if (status_ == 200) {
      readCollection();
    } else {
      finishSync(Sync::Failed);
    }
    return true;
  }

  // While it works, the idle timer waits: a sleep would cut the radio mid-download.
  bool busy() const override { return sync_ == Sync::Waiting || sync_ == Sync::Downloading || sync_ == Sync::Stopping; }

  // Back from the Wi-Fi screen, which closes by itself once it joins.
  void onWifiClosed() override {
    if (sync_ != Sync::Joining) return;
    if (network::state() == network::State::Connected) {
      startWork();
    } else {
      // BACK during a join leaves the radio still trying, so stop it.
      network::turnOff();
      sync_ = Sync::NoWifi;
    }
  }

  // Nothing is saved yet: the games are made up, so there's nothing worth keeping.
  bool saveFailed() const override { return false; }
  void retrySave() override {}

 private:
  int pageCount() const { return static_cast<int>(pages_.size()); }
  int firstRow() const { return pages_[page_]; }
  int endRow() const { return page_ + 1 < pageCount() ? pages_[page_ + 1] : static_cast<int>(games_.size()); }

  // A new list: every row measured once, then split into pages, back on the first.
  void showGames(std::vector<bgg::Game> games) {
    games_ = std::move(games);
    heights_.clear();
    for (const bgg::Game& game : games_) heights_.push_back(rowHeight(game));
    pages_ = bgg::paginate(heights_, LIST_ROOM, ui::ROW_GAP);
    page_ = 0;
    selected_ = -1;
  }

  // The rows stack from the top of the list, so a row's top is the sum of the ones above it on
  // its page. Drawing and tapping both ask this, so they can't drift apart.
  int rowTop(int row) const {
    int top = LIST_TOP;
    for (int r = firstRow(); r < row; r++) top += heights_[r] + ui::ROW_GAP;
    return top;
  }

  enum class Sync {
    Never,
    ConfigProblem,
    Joining,
    Downloading,
    Waiting,  // BGG answered 202, so a wait before asking again
    Stopping,
    Synced,
    StillQueued,  // still 202 after every attempt
    BgError,      // BGG answered with an error, such as an unknown username
    BadAnswer,    // an answer the parser couldn't read, such as one cut off
    TokenRefused,
    Failed,
    NoWifi,
    NoClock,
  };

  // SYNC: the config first, so a missing or broken file never turns the radio on. Then straight
  // to work if Wi-Fi is already up, otherwise the Wi-Fi screen first.
  TapResult startSync() {
    if (sync_ == Sync::Joining || sync_ == Sync::Waiting || sync_ == Sync::Downloading || sync_ == Sync::Stopping) {
      return TapResult::Nothing;
    }
    configError_ = readConfig();
    if (configError_ != bgg::ConfigError::None) {
      sync_ = Sync::ConfigProblem;
      return TapResult::Redraw;
    }
    if (network::state() == network::State::Connected) {
      startWork();
      return TapResult::Redraw;
    }
    sync_ = Sync::Joining;
    return TapResult::OpenWifi;
  }

  // Read on each SYNC rather than at boot, so a corrected file counts at once, and the token is
  // in memory only while it's needed.
  bgg::ConfigError readConfig() {
    size_t size = 0;
    storage::Blob file = storage::readFile(bgg::kConfigPath, bgg::kMaxConfigBytes, size);
    if (!file) {
      // readFile() says nothing for a missing file and one too big, so look for it.
      for (const std::string& name : storage::listFolder(bgg::kConfigFolder, 64)) {
        if (strcasecmp(name.c_str(), bgg::kConfigFile) == 0) return bgg::ConfigError::Unreadable;
      }
      return bgg::ConfigError::Missing;
    }
    const bgg::ConfigError error = bgg::parseConfig(reinterpret_cast<const char*>(file.get()), size, config_);
    std::memset(file.get(), 0, size);  // the file's copy of the token, before the memory is freed
    if (error == bgg::ConfigError::None) {
      // The token's length helps spot a bad paste; the token itself is never logged.
      Serial.printf("PocketBGG config: username %s, token found (%u characters)\n", config_.username.c_str(),
                    static_cast<unsigned>(config_.token.size()));
    } else {
      Serial.printf("PocketBGG config: %s\n", bgg::configErrorText(error));
    }
    return error;
  }

  void startWork() {
    // A certificate is valid between two dates, so checking one needs the real date. Without
    // it every certificate would look expired or not yet valid, and the token mustn't be sent
    // unchecked.
    if (!wallClock::isSet()) {
      finishSync(Sync::NoClock);
      return;
    }
    url_ = std::string(COLLECTION_URL) + bgg::urlEncode(config_.username);
    authorization_ = "Bearer " + config_.token;
    bgg::wipe(config_.token);  // now only in the header, which every attempt reuses
    attempt_ = 0;
    startAttempt();
  }

  void startAttempt() {
    attempt_++;
    answer_.clear();
    answer_.reserve(EXPECTED_ANSWER_BYTES);
    received_ = 0;
    shownBytes_ = 0;
    status_ = 0;

    download::Options options;
    options.rootCa = bgg::kBggRootCa;
    options.headerName = "Authorization";
    options.headerValue = authorization_.c_str();
    sync_ = Sync::Downloading;
    if (!download::start(url_.c_str(), download::Sink{this, keepAnswer, dropAnswer}, options)) finishSync(Sync::Failed);
  }

  // In loop(), once the download has ended: the parse is quick (7 ms for 143 games on a PC), so
  // it runs here rather than in a task of its own.
  void readCollection() {
    bgg::Collection collection;
    const bgg::ParseResult result = bgg::parseCollection(answer_.data(), answer_.size(), collection);
    std::string().swap(answer_);  // give the PSRAM back
    switch (result) {
      case bgg::ParseResult::Ok:
        Serial.printf("PocketBGG: %u games, of %u BGG says it sent\n", static_cast<unsigned>(collection.games.size()),
                      static_cast<unsigned>(collection.totalItems));
        showGames(std::move(collection.games));
        synced_ = true;
        finishSync(Sync::Synced);
        return;
      case bgg::ParseResult::BgError:
        bgError_ = collection.error;
        Serial.printf("PocketBGG: BGG said \"%s\"\n", bgError_.c_str());
        finishSync(Sync::BgError);
        return;
      case bgg::ParseResult::NotCollection:
      case bgg::ParseResult::Incomplete:
        Serial.printf("PocketBGG: the answer couldn't be read (%s)\n", result == bgg::ParseResult::Incomplete ? "cut off" : "not a collection");
        finishSync(Sync::BadAnswer);
        return;
    }
  }

  // The download task hands BGG's answer over here, in memory only: the card and panel share a
  // bus, so nothing goes to the card from that task.
  static bool keepAnswer(void* context, const uint8_t* data, size_t size) {
    PocketBgg& app = *static_cast<PocketBgg*>(context);
    if (app.answer_.size() + size > MAX_ANSWER_BYTES) return false;
    app.answer_.append(reinterpret_cast<const char*>(data), size);
    app.received_ = app.answer_.size();
    return true;
  }
  static void dropAnswer(void* context) {
    PocketBgg& app = *static_cast<PocketBgg*>(context);
    app.answer_.clear();
    app.received_ = 0;
  }

  // Done with the network, so the radio goes off: a running radio drains the battery. And done
  // with the token.
  void finishSync(Sync result) {
    network::turnOff();
    bgg::wipe(authorization_);
    bgg::wipe(config_.token);
    std::string().swap(answer_);
    sync_ = result;
  }

  // Leaving while joining or between attempts: no download is running, so the radio can go.
  void stopSync() {
    if (sync_ != Sync::Joining && sync_ != Sync::Waiting) return;
    finishSync(Sync::Never);
  }

  const char* statusText(char* out, size_t size) const {
    switch (sync_) {
      case Sync::Never:
        return "NOT SYNCED YET";
      case Sync::Joining:
        return "JOINING WI-FI";
      case Sync::ConfigProblem:
        return bgg::configErrorText(configError_);
      case Sync::Downloading:
        if (received_.load() == 0) {
          std::snprintf(out, size, attempt_ == 1 ? "ASKING BGG FOR YOUR COLLECTION" : "ASKING AGAIN (%d OF %d)", attempt_, MAX_ATTEMPTS);
        } else {
          std::snprintf(out, size, "DOWNLOADING, %u KB", static_cast<unsigned>(received_.load() / 1024));
        }
        return out;
      case Sync::Waiting:
        std::snprintf(out, size, "BGG IS PREPARING IT (%d OF %d)", attempt_, MAX_ATTEMPTS);
        return out;
      case Sync::Stopping:
        return "STOPPING";
      case Sync::Synced:
        std::snprintf(out, size, "SYNCED %u GAMES", static_cast<unsigned>(games_.size()));
        return out;
      case Sync::StillQueued:
        return "BGG IS STILL PREPARING IT. TRY AGAIN SOON";
      case Sync::BgError:
        std::snprintf(out, size, "BGG: %s", bgError_.c_str());
        return textWidth(Font::Size10, out) <= INNER_WIDTH ? out : "BGG ANSWERED WITH AN ERROR";
      case Sync::BadAnswer:
        return "BGG'S ANSWER COULDN'T BE READ";
      case Sync::TokenRefused:
        std::snprintf(out, size, "BGG REFUSED THE TOKEN (HTTP %d)", status_);
        return out;
      case Sync::Failed:
        if (status_ == 0) return "NO ANSWER FROM BGG";
        std::snprintf(out, size, "BGG ANSWERED HTTP %d", status_);
        return out;
      case Sync::NoClock:
        return "CLOCK NOT SET: TOKEN NOT SENT";
      case Sync::NoWifi:
        return "NO WI-FI, SO NOT SYNCED";
    }
    return "";
  }

  void drawActions(uint8_t* frame) const {
    char buffer[96];
    const char* status = statusText(buffer, sizeof(buffer));
    // Size14, stepping down a cut for a long username rather than cutting it short.
    const Font font = textWidth(Font::Size14, status) <= INNER_WIDTH ? Font::Size14 : Font::Size10;
    drawTextInBox(frame, font, MARGIN, STATUS_TOP, INNER_WIDTH, STATUS_HEIGHT, status);
    if (sync_ == Sync::Joining || sync_ == Sync::Waiting || sync_ == Sync::Downloading || sync_ == Sync::Stopping) {
      ui::drawDimmedPill(frame, MARGIN, BUTTON_TOP, INNER_WIDTH, "SYNCING");
    } else {
      ui::drawPill(frame, MARGIN, BUTTON_TOP, INNER_WIDTH, "SYNC", true);
    }
  }

  TapResult turnPage(int step) {
    const int next = page_ + step;
    if (next < 0 || next >= pageCount()) return TapResult::Nothing;
    page_ = next;
    return TapResult::Redraw;
  }

  // How many games, or "SAMPLE DATA" before the first sync so made-up games are never taken
  // for a real collection, and which page this is on the right.
  void drawCaption(uint8_t* frame, bool withPage) const {
    char count[24];
    if (synced_) std::snprintf(count, sizeof(count), "%u GAMES", static_cast<unsigned>(games_.size()));
    ui::drawTextLeft(frame, Font::Size14, MARGIN, CAPTION_TOP, CAPTION_HEIGHT, synced_ ? count : "SAMPLE DATA");
    if (!withPage) return;
    char page[24];
    std::snprintf(page, sizeof(page), "PAGE %d OF %d", page_ + 1, pageCount());
    const int width = textWidth(Font::Size14, page);
    ui::drawTextLeft(frame, Font::Size14, MARGIN + INNER_WIDTH - width, CAPTION_TOP, CAPTION_HEIGHT, page);
  }

  void drawList(uint8_t* frame) const {
    drawCaption(frame, true);
    for (int row = firstRow(); row < endRow(); row++) {
      const bgg::Game& game = games_[row];
      const int top = rowTop(row);
      drawRectOutline(frame, MARGIN, top, INNER_WIDTH, heights_[row], ui::HAIRLINE);
      const int nameHeight = drawWrapped(frame, NAME_FONT, MARGIN + ROW_INSET, top + ROW_PAD_TOP, NAME_WIDTH, game.name.c_str());
      char meta[40];
      bgg::describe(game, meta, sizeof(meta));
      ui::drawTextLeft(frame, META_FONT, MARGIN + ROW_INSET, top + ROW_PAD_TOP + nameHeight, META_HEIGHT, meta);
    }
    drawActions(frame);
  }

  void drawDetails(uint8_t* frame, const bgg::Game& game) const {
    drawCaption(frame, false);
    // Where the thumbnail goes once the app can download one (M6).
    ui::drawDimmedPill(frame, THUMB_LEFT, THUMB_TOP, THUMB, "", THUMB);
    drawTextInBox(frame, Font::Size14, THUMB_LEFT, THUMB_TOP, THUMB, THUMB, "NO IMAGE YET");

    // The name as large as fits in three lines, never cut short.
    Font font = Font::Size30;
    if (wrappedHeight(font, INNER_WIDTH, game.name.c_str()) > 3 * lineHeight(font)) font = Font::Size20;
    const int nameHeight = drawWrapped(frame, font, MARGIN, DETAIL_NAME_TOP, INNER_WIDTH, game.name.c_str(), Align::Center);

    char meta[40];
    bgg::describe(game, meta, sizeof(meta));
    const int metaTop = DETAIL_NAME_TOP + nameHeight + GAP;
    const Font metaFont = textWidth(Font::Size20, meta) <= INNER_WIDTH ? Font::Size20 : Font::Size14;
    drawTextInBox(frame, metaFont, MARGIN, metaTop, INNER_WIDTH, lineHeight(Font::Size20), meta);

    // Players and play time, when BGG gives them, then the id.
    char line[48];
    int lineTop = metaTop + lineHeight(Font::Size20) + 4;
    if (players(game, line, sizeof(line))) {
      drawTextInBox(frame, Font::Size14, MARGIN, lineTop, INNER_WIDTH, lineHeight(Font::Size14), line);
      lineTop += lineHeight(Font::Size14);
    }
    std::snprintf(line, sizeof(line), "BGG ID %u", static_cast<unsigned>(game.id));
    drawTextInBox(frame, Font::Size14, MARGIN, lineTop, INNER_WIDTH, lineHeight(Font::Size14), line);
  }

  // "2-7 PLAYERS  -  30 MIN", or as much of it as BGG gave. False if it gave none.
  static bool players(const bgg::Game& game, char* out, size_t size) {
    char who[24] = "", time[16] = "";
    if (game.minPlayers > 0 && game.maxPlayers > game.minPlayers) {
      std::snprintf(who, sizeof(who), "%d-%d PLAYERS", game.minPlayers, game.maxPlayers);
    } else if (game.minPlayers > 0) {
      std::snprintf(who, sizeof(who), "%d %s", game.minPlayers, game.minPlayers == 1 ? "PLAYER" : "PLAYERS");
    }
    if (game.playTime > 0) std::snprintf(time, sizeof(time), "%u MIN", static_cast<unsigned>(game.playTime));
    if (who[0] == '\0' && time[0] == '\0') return false;
    std::snprintf(out, size, "%s%s%s", who, who[0] != '\0' && time[0] != '\0' ? "  -  " : "", time);
    return true;
  }

  std::vector<bgg::Game> games_;  // the sample until the first sync, then the collection
  bool synced_ = false;
  std::vector<int> heights_;  // each row's height, measured once
  std::vector<int> pages_;    // the first row of each page
  int page_ = 0;
  int selected_ = -1;  // the game whose details are showing, or -1 for the list
  Sync sync_ = Sync::Never;
  bgg::ConfigError configError_ = bgg::ConfigError::None;
  bgg::Config config_;         // the token is wiped as soon as it's in the header
  std::string url_;            // these three must outlive the download
  std::string authorization_;  // "Bearer <token>", wiped when the request ends
  std::string answer_;               // the download task's alone until it ends
  std::atomic<size_t> received_{0};  // answer_'s size, safe to read from loop() meanwhile
  size_t shownBytes_ = 0;
  int status_ = 0;
  int attempt_ = 0;
  uint32_t waitStartMs_ = 0;
  std::string bgError_;
};

}  // namespace

App& pocketBggApp() {
  static PocketBgg app;
  return app;
}
