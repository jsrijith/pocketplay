#include "pocketbgg.h"

#include <Arduino.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <ctime>
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
#include "pocketbgg_filter.h"
#include "pocketbgg_icon.h"
#include "pocketbgg_image.h"
#include "pocketbgg_plays.h"
#include "pocketbgg_list.h"
#include "pocketbgg_save.h"
#include "pocketbgg_thumbs.h"
#include "ui/ui.h"

namespace bgg = pocketbgg;

namespace {

// The list: a caption line saying how many games and which page this is, then one row per
// game (the name, wrapped, and a line under it with the plays, status and last-played date),
// then a status line and FILTER and SYNC along the bottom. The Up and Down buttons turn the
// pages. The details: the same caption with which game this is, the game's cover, the name
// large, the year, plays and status, players and time, and the date last played; Up and Down
// step through the games.
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

// The bottom row holds the actions, FILTER and SYNC. The status line above it says how the last
// sync went.
constexpr int BUTTON_TOP = SCREEN_HEIGHT - MARGIN - ui::PILL_HEIGHT;
constexpr int BUTTON_WIDTH = (INNER_WIDTH - GAP) / 2;
constexpr int FILTER_LEFT = MARGIN;
constexpr int SYNC_LEFT = MARGIN + BUTTON_WIDTH + GAP;
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
// The answer goes to PSRAM: malloc puts anything over 4 KB there. 203 games came to 274 KB, so
// this reserves room for more and stops at 1 MB, about 750 games.
constexpr size_t EXPECTED_ANSWER_BYTES = 384 * 1024;
constexpr size_t MAX_ANSWER_BYTES = 1024 * 1024;
constexpr size_t PROGRESS_STEP_BYTES = 64 * 1024;  // a redraw each time this much more has come

// After the collection, every page of plays, for each game's last-played date: 100 plays a page,
// about 63 KB, with RETRY_WAIT_MS between pages, as BGG asks of its users.
constexpr const char* PLAYS_URL = "https://boardgamegeek.com/xmlapi2/plays?username=";
constexpr size_t EXPECTED_PAGE_BYTES = 96 * 1024;

// The filter screen: a row for each status, then played only, the player count and the order.
// Eleven rows of 52 with gaps of 4 fit between the caption and the bottom margin.
constexpr int FILTER_ROW_HEIGHT = 52;
constexpr int FILTER_ROWS = bgg::kStatusCount + 3;
constexpr int PLAYED_ROW = bgg::kStatusCount;
constexpr int PLAYERS_ROW = bgg::kStatusCount + 1;
constexpr int ORDER_ROW = bgg::kStatusCount + 2;
constexpr int BOX = 28;  // the on/off box at a row's right
static_assert(LIST_TOP + FILTER_ROWS * (FILTER_ROW_HEIGHT + ui::ROW_GAP) - ui::ROW_GAP <= SCREEN_HEIGHT - MARGIN,
              "the filter rows must fit above the bottom margin");

int filterRowTop(int row) { return LIST_TOP + row * (FILTER_ROW_HEIGHT + ui::ROW_GAP); }

constexpr int THUMB = 216;  // BGG's thumbnails are up to 200 x 150, so this leaves a border
constexpr int THUMB_LEFT = (SCREEN_WIDTH - THUMB) / 2;
constexpr int THUMB_TOP = LIST_TOP;
constexpr int DETAIL_NAME_TOP = THUMB_TOP + THUMB + 2 * GAP;
constexpr int PICTURE_BOX = 200;  // the picture fits inside this, in the middle of the box

// Last in a sync, each game's cover from BGG's image server, a CDN: no token, so no pause
// between them as the XML API asks. Its certificate must lead to Amazon's root all the same, so
// nothing else can hand the decoder a file. A cover is about 10 KB.
constexpr size_t EXPECTED_COVER_BYTES = 32 * 1024;
// The list's status line is redrawn this often while covers come, not after every one.
constexpr uint32_t COVER_PROGRESS_MS = 10000;
// Covers that got no answer at all, one after another, mean Wi-Fi has gone: stop rather than try
// every other game.
constexpr int MAX_COVERS_UNANSWERED = 5;

int rowHeight(const bgg::Game& game) {
  return ROW_PAD_TOP + wrappedHeight(NAME_FONT, NAME_WIDTH, game.name.c_str()) + META_HEIGHT + ROW_PAD_BOTTOM;
}

class PocketBgg final : public App {
 public:
  const char* name() const override { return "POCKETBGG"; }
  const uint8_t* icon() const override { return pocketbggIcon::APP; }

  void open() override {
    // So the player can see where config.txt goes when the card is in a PC. Only the folder:
    // the file is theirs to write.
    if (storage::ready()) storage::ensureFolder(bgg::kConfigFolder);
    if (loaded_) return;
    loaded_ = true;
    loadFilter();
    showGames({});  // one empty page until a list is loaded or synced, so there's always a page 1
    loadList();
  }

  TapResult onTap(int x, int y) override {
    if (view_ == View::Filters) return tapFilters(x, y);
    if (ui::isOnBack(x, y)) {
      if (view_ == View::Details) {
        // Back to the page holding the game shown last, which Up and Down may have moved on
        // from the one tapped. Dates that came in meanwhile may have changed the order, so it's
        // found again by its place in games_.
        const int game = visible_[selected_];
        if (needsRefresh_) applyFilter();
        view_ = View::List;
        for (int row = 0; row < static_cast<int>(visible_.size()); row++) {
          if (visible_[row] == game) page_ = bgg::pageOf(pages_, row);
        }
        return TapResult::RedrawFull;
      }
      // A download has to stop before the radio can go off, so the first BACK asks it to, and
      // the app stays until it has.
      if (sync_ == Sync::Downloading || sync_ == Sync::Stopping) {
        if (sync_ == Sync::Stopping) return TapResult::Nothing;
        download::requestCancel();
        sync_ = Sync::Stopping;
        return TapResult::Redraw;
      }
      stopSync();
      return TapResult::Leave;
    }
    if (view_ == View::Details) return TapResult::Nothing;

    if (ui::isInBox(x, y, SYNC_LEFT, BUTTON_TOP, BUTTON_WIDTH, ui::PILL_HEIGHT)) return startSync();
    if (ui::isInBox(x, y, FILTER_LEFT, BUTTON_TOP, BUTTON_WIDTH, ui::PILL_HEIGHT)) {
      if (games_.empty()) return TapResult::Nothing;
      view_ = View::Filters;
      return TapResult::RedrawFull;
    }

    for (int row = firstRow(); row < endRow(); row++) {
      if (ui::isInBox(x, y, MARGIN, rowTop(row), INNER_WIDTH, heights_[row])) {
        selected_ = row;
        view_ = View::Details;
        loadCover(games_[visible_[row]]);
        return TapResult::RedrawFull;
      }
    }
    return TapResult::Nothing;
  }

  // Up and Down turn the list's pages, and on a game's details step to the game before or
  // after it in the list as it's shown. Both do nothing past the first or last.
  TapResult onUpPressed() override { return step(-1); }
  TapResult onDownPressed() override { return step(1); }

  void draw(uint8_t* frame) override {
    ui::drawBand(frame, name(), nullptr, true);
    switch (view_) {
      case View::List:
        drawList(frame);
        break;
      case View::Details:
        drawDetails(frame, games_[visible_[selected_]]);
        break;
      case View::Filters:
        drawFilters(frame);
        break;
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
    if (phase_ == Phase::Covers) return readCover();
    status_ = download::httpStatus();
    Serial.printf("PocketBGG: attempt %d, HTTP %d, %u bytes\n", attempt_, status_, static_cast<unsigned>(answer_.size()));
    if (status_ == 202) {
      if (attempt_ >= MAX_ATTEMPTS) {
        finishSync(Sync::StillQueued);
      } else {
        sync_ = Sync::Waiting;
        waitingForBgg_ = true;
        waitStartMs_ = millis();
      }
      return true;
    }
    if (status_ == 401 || status_ == 403) {
      finishSync(Sync::TokenRefused);
    } else if (status_ == 200) {
      if (phase_ == Phase::Plays) {
        readPlaysPage();
      } else {
        readCollection();
      }
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

  bool saveFailed() const override { return saveFailed_ || filterSaveFailed_; }
  void retrySave() override {
    if (saveFailed_) saveList();
    if (filterSaveFailed_) saveFilter();
  }

 private:
  int pageCount() const { return static_cast<int>(pages_.size()); }
  int firstRow() const { return pages_[page_]; }
  int endRow() const { return page_ + 1 < pageCount() ? pages_[page_ + 1] : static_cast<int>(visible_.size()); }

  enum class View { List, Details, Filters };
  enum class Phase { Collection, Plays, Covers };

  // A new collection, shown through the filter.
  void showGames(std::vector<bgg::Game> games) {
    games_ = std::move(games);
    applyFilter();
  }

  // The games the filter lets through, each row measured once, then split into pages, back on
  // the first. Rows count positions in visible_, which point into games_.
  void applyFilter() {
    needsRefresh_ = false;
    visible_ = bgg::visibleGames(games_, filter_);
    heights_.clear();
    for (const int i : visible_) heights_.push_back(rowHeight(games_[i]));
    pages_ = bgg::paginate(heights_, LIST_ROOM, ui::ROW_GAP);
    page_ = 0;
    selected_ = -1;
    view_ = View::List;
  }

  // --- The filter screen ---

  TapResult tapFilters(int x, int y) {
    if (ui::isOnBack(x, y)) {
      applyFilter();
      return TapResult::RedrawFull;
    }
    for (int row = 0; row < FILTER_ROWS; row++) {
      if (!ui::isInBox(x, y, MARGIN, filterRowTop(row), INNER_WIDTH, FILTER_ROW_HEIGHT)) continue;
      if (row == PLAYED_ROW) {
        filter_.playedOnly = !filter_.playedOnly;
      } else if (row == PLAYERS_ROW) {
        filter_.players = bgg::nextPlayers(filter_.players);
      } else if (row == ORDER_ROW) {
        filter_.order = filter_.order == bgg::Order::Name ? bgg::Order::LastPlayed : bgg::Order::Name;
      } else if (!bgg::toggleStatus(filter_, bgg::kStatusNames[row].flag)) {
        return TapResult::Nothing;  // the last status chosen stays on
      }
      saveFilter();
      return TapResult::Redraw;
    }
    return TapResult::Nothing;
  }

  void drawFilters(uint8_t* frame) const {
    // How many games these choices show, so a change can be judged before leaving.
    char caption[40];
    const size_t shown = bgg::visibleGames(games_, filter_).size();
    std::snprintf(caption, sizeof(caption), "SHOWING %u OF %u", static_cast<unsigned>(shown), static_cast<unsigned>(games_.size()));
    ui::drawTextLeft(frame, Font::Size14, MARGIN, CAPTION_TOP, CAPTION_HEIGHT, caption);

    char count[16];
    for (int row = 0; row < bgg::kStatusCount; row++) {
      const bgg::StatusName& status = bgg::kStatusNames[row];
      std::snprintf(count, sizeof(count), "%d", bgg::countWithStatus(games_, status.flag));
      drawFilterRow(frame, row, status.label, count, (filter_.statuses & status.flag) != 0);
    }
    drawFilterRow(frame, PLAYED_ROW, "PLAYED AT LEAST ONCE", "", filter_.playedOnly);

    char players[8];
    bgg::playersLabel(filter_.players, players, sizeof(players));
    drawChoiceRow(frame, PLAYERS_ROW, "PLAYERS", players);
    drawChoiceRow(frame, ORDER_ROW, "ORDER", bgg::orderLabel(filter_.order));
  }

  // A label on the left and the choice a tap steps on the right.
  static void drawChoiceRow(uint8_t* frame, int row, const char* label, const char* choice) {
    const int top = filterRowTop(row);
    drawRectOutline(frame, MARGIN, top, INNER_WIDTH, FILTER_ROW_HEIGHT, ui::HAIRLINE);
    ui::drawTextLeft(frame, Font::Size20, MARGIN + ROW_INSET, top, FILTER_ROW_HEIGHT, label);
    const int width = textWidth(Font::Size20, choice);
    ui::drawTextLeft(frame, Font::Size20, MARGIN + INNER_WIDTH - ROW_INSET - width, top, FILTER_ROW_HEIGHT, choice);
  }

  // A label, a count beside the box, and the box: an outline when off, filled inside when on.
  static void drawFilterRow(uint8_t* frame, int row, const char* label, const char* count, bool on) {
    const int top = filterRowTop(row);
    drawRectOutline(frame, MARGIN, top, INNER_WIDTH, FILTER_ROW_HEIGHT, ui::HAIRLINE);
    ui::drawTextLeft(frame, Font::Size20, MARGIN + ROW_INSET, top, FILTER_ROW_HEIGHT, label);
    const int boxLeft = MARGIN + INNER_WIDTH - ROW_INSET - BOX;
    const int boxTop = top + (FILTER_ROW_HEIGHT - BOX) / 2;
    if (count[0] != '\0') {
      const int width = textWidth(Font::Size14, count);
      ui::drawTextLeft(frame, Font::Size14, boxLeft - GAP - width, top, FILTER_ROW_HEIGHT, count);
    }
    drawRectOutline(frame, boxLeft, boxTop, BOX, BOX, ui::RULE);
    if (on) fillRect(frame, boxLeft + 7, boxTop + 7, BOX - 14, BOX - 14, true);
  }

  void loadFilter() {
    if (!storage::ready()) return;
    uint8_t data[bgg::kFilterBytes];
    uint8_t version = 0;
    size_t size = 0;
    if (storage::load(bgg::kFilterSave, version, data, sizeof(data), size) && !bgg::unpackFilter(version, data, size, filter_)) {
      Serial.println("PocketBGG: ignoring the saved filter");
    }
  }

  // On every change, as the save rules ask.
  void saveFilter() {
    if (!storage::ready()) {
      filterSaveFailed_ = false;
      return;
    }
    uint8_t data[bgg::kFilterBytes];
    const size_t size = bgg::packFilter(filter_, data);
    filterSaveFailed_ = !storage::save(bgg::kFilterSave, bgg::kFilterVersion, data, size);
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
    PlaysStopped,  // the collection synced, but not every page of plays
    CoversStopped,  // the collection and plays synced, but not every cover
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
    username_ = config_.username;
    coversFailed_ = 0;
    url_ = std::string(COLLECTION_URL) + bgg::urlEncode(username_);
    authorization_ = "Bearer " + config_.token;
    bgg::wipe(config_.token);  // now only in the header, which every request reuses
    phase_ = Phase::Collection;
    attempt_ = 0;
    startAttempt();
  }

  void startAttempt() {
    attempt_++;
    answer_.clear();
    answer_.reserve(phase_ == Phase::Plays ? EXPECTED_PAGE_BYTES : EXPECTED_ANSWER_BYTES);
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

  // The list as it was last synced, from the card, with no network. A damaged save is ignored,
  // so the app starts empty rather than half-restored.
  void loadList() {
    if (!storage::ready()) return;
    size_t size = 0;
    const storage::Blob blob = storage::loadBlob(bgg::kListSave, size);
    if (!blob) return;
    bgg::SavedList list;
    if (!bgg::unpackList(blob.get(), size, list)) {
      Serial.println("PocketBGG: ignoring the saved list, which couldn't be read");
      return;
    }
    Serial.printf("PocketBGG: %u games from the card, %u bytes\n", static_cast<unsigned>(list.games.size()), static_cast<unsigned>(size));
    syncedAt_ = list.syncedAt;
    synced_ = true;
    showGames(std::move(list.games));
  }

  // Straight after a sync, in loop(): the card shares the panel's bus, so nothing is written from
  // the download task. saveBlob() writes a temp file, reads it back and then replaces the old
  // one, so a power cut leaves the old list or the new, never a broken one.
  void saveList() {
    if (!storage::ready()) {
      saveFailed_ = false;
      return;
    }
    bgg::SavedList list;
    list.syncedAt = syncedAt_;
    list.games = games_;
    std::vector<uint8_t> bytes;
    if (!bgg::packList(list, bytes)) {
      // Something the parser let through that the save can't hold. Kept on screen, not saved.
      Serial.println("PocketBGG: the list couldn't be packed to save");
      saveFailed_ = true;
      return;
    }
    saveFailed_ = !storage::saveBlob(bgg::kListSave, bytes.data(), bytes.size());
    Serial.printf("PocketBGG: saved %u bytes%s\n", static_cast<unsigned>(bytes.size()), saveFailed_ ? ", FAILED" : "");
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
        // Until the plays have been read again, each game keeps the date it had.
        bgg::carryLastPlayed(collection.games, games_);
        showGames(std::move(collection.games));
        synced_ = true;
        // The clock is set: the certificate check needed it.
        syncedAt_ = static_cast<uint32_t>(std::time(nullptr));
        saveList();
        startPlays();
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

  // --- The plays, for last-played dates ---

  void startPlays() {
    phase_ = Phase::Plays;
    latest_.clear();
    playsPage_ = 1;
    playsPages_ = 0;
    nextPage();
  }

  // The page after a pause, so BGG isn't asked too quickly; the first straight away.
  void nextPage() {
    url_ = std::string(PLAYS_URL) + bgg::urlEncode(username_) + "&page=" + std::to_string(playsPage_);
    attempt_ = 0;
    if (playsPage_ == 1) {
      startAttempt();
    } else {
      sync_ = Sync::Waiting;
      waitingForBgg_ = false;
      waitStartMs_ = millis();
    }
  }

  // In loop(), once a page has come. Pages come newest first, so a game's date is final as soon
  // as it's seen, and it's shown straight away.
  void readPlaysPage() {
    bgg::PlaysPage page;
    const bgg::PlaysResult result = bgg::parsePlaysPage(answer_.data(), answer_.size(), page);
    std::string().swap(answer_);
    if (result != bgg::PlaysResult::Ok) {
      Serial.printf("PocketBGG: plays page %d couldn't be read (%d)\n", playsPage_, static_cast<int>(result));
      finishSync(Sync::PlaysStopped);
      return;
    }
    bgg::addPlays(page, latest_);
    playsPages_ = bgg::playsPages(page.total);
    const bool complete = playsPage_ >= playsPages_;
    bgg::applyLastPlayed(games_, latest_, complete);
    Serial.printf("PocketBGG: plays page %d of %d, %u plays\n", playsPage_, playsPages_, static_cast<unsigned>(page.plays.size()));
    if (!complete) {
      playsPage_++;
      nextPage();
      return;
    }
    // latest_ holds every game played, some not in the collection, so count the collection's.
    unsigned dated = 0;
    for (const bgg::Game& g : games_) dated += g.lastPlayed != 0;
    Serial.printf("PocketBGG: last-played dates for %u of %u games\n", dated, static_cast<unsigned>(games_.size()));
    saveList();
    refreshOrder();
    startCovers();
  }

  // --- The covers ---

  // The covers saved on the card, from one listing of the save folder: each one's name says
  // which game and which picture it is.
  static std::vector<bgg::SavedThumb> savedCovers() {
    std::vector<bgg::SavedThumb> saved;
    bgg::SavedThumb thumb;
    for (const std::string& file : storage::listFolder(storage::SAVE_FOLDER, 8192)) {
      if (bgg::parseThumbFile(file, thumb)) saved.push_back(thumb);
    }
    return saved;
  }

  // Every game whose cover isn't saved yet, or has changed on BGG. A sync that stopped partway
  // carries on here next time, with only what's still missing.
  void startCovers() {
    phase_ = Phase::Covers;
    bgg::wipe(authorization_);  // the image server needs no token, so it's done with
    covers_.clear();
    const std::vector<bgg::SavedThumb> saved = savedCovers();
    for (const int i : bgg::thumbsToFetch(games_, saved)) {
      covers_.push_back(Cover{games_[i].id, bgg::picId(games_[i].thumbnail), games_[i].thumbnail});
    }
    coverAt_ = 0;
    coversFetched_ = 0;
    coversUnanswered_ = 0;
    coversStartMs_ = millis();
    lastCoverDrawMs_ = millis();
    Serial.printf("PocketBGG: %u covers to fetch, %u saved already\n", static_cast<unsigned>(covers_.size()),
                  static_cast<unsigned>(saved.size()));
    if (covers_.empty()) {
      finishCovers();
    } else {
      startCover();
    }
  }

  void startCover() {
    answer_.clear();
    answer_.reserve(EXPECTED_COVER_BYTES);
    received_ = 0;
    shownBytes_ = 0;
    status_ = 0;
    url_ = covers_[coverAt_].url;
    coverStartMs_ = millis();

    download::Options options;
    options.rootCa = bgg::kAmazonRootCa;
    sync_ = Sync::Downloading;
    if (!download::start(url_.c_str(), download::Sink{this, keepAnswer, dropAnswer}, options)) finishSync(Sync::Failed);
  }

  // In loop(), once a cover has come: decoded, dithered and saved straight away, so a sync
  // stopped later keeps it. One that can't be had is counted and skipped. True asks for a redraw.
  bool readCover() {
    const Cover& cover = covers_[coverAt_];
    status_ = download::httpStatus();
    const unsigned fetchMs = static_cast<unsigned>(millis() - coverStartMs_);
    bool redraw = false;
    if (status_ == 200) {
      coversUnanswered_ = 0;
      const uint32_t decodeStartMs = millis();
      bgg::Picture picture;
      const bgg::PictureResult result = bgg::makePicture(reinterpret_cast<const uint8_t*>(answer_.data()), answer_.size(),
                                                         PICTURE_BOX, PICTURE_BOX, picture);
      const unsigned decodeMs = static_cast<unsigned>(millis() - decodeStartMs);
      std::vector<uint8_t> bytes;
      if (result == bgg::PictureResult::Ok) bgg::packPicture(picture, bytes);
      if (result == bgg::PictureResult::Ok && storage::saveBlob(bgg::thumbName(cover.gameId, cover.pic).c_str(), bytes.data(), bytes.size())) {
        coversFetched_++;
        Serial.printf("PocketBGG: cover %u of %u, game %u, %u bytes in %u ms, decoded in %u ms\n", coverAt_ + 1,
                      static_cast<unsigned>(covers_.size()), static_cast<unsigned>(cover.gameId),
                      static_cast<unsigned>(answer_.size()), fetchMs, decodeMs);
        // The game on screen got its cover: show it now.
        if (view_ == View::Details && games_[visible_[selected_]].id == cover.gameId) {
          picture_ = std::move(picture);
          redraw = true;
        }
      } else {
        coversFailed_++;
        Serial.printf("PocketBGG: cover for game %u: %s\n", static_cast<unsigned>(cover.gameId),
                      result == bgg::PictureResult::Ok ? "couldn't be saved" : bgg::pictureErrorText(result));
      }
    } else {
      coversFailed_++;
      if (status_ == 0) coversUnanswered_++;
      Serial.printf("PocketBGG: cover for game %u: HTTP %d\n", static_cast<unsigned>(cover.gameId), status_);
    }
    std::string().swap(answer_);

    if (coversUnanswered_ >= MAX_COVERS_UNANSWERED) {
      finishSync(Sync::Failed);
      return true;
    }
    coverAt_++;
    if (coverAt_ >= static_cast<int>(covers_.size())) {
      finishCovers();
      return true;
    }
    startCover();
    // The status line shows on the list only, and every few seconds is enough.
    if (view_ == View::List && millis() - lastCoverDrawMs_ >= COVER_PROGRESS_MS) {
      lastCoverDrawMs_ = millis();
      redraw = true;
    }
    return redraw;
  }

  // After every cover has been tried: the saved ones no game shows any more are deleted, the
  // covers of games that left the collection and the old ones of games whose cover changed.
  void finishCovers() {
    unsigned deleted = 0;
    for (const bgg::SavedThumb& old : bgg::staleThumbs(games_, savedCovers())) {
      deleted += storage::eraseBlob(bgg::thumbName(old.gameId, old.pic).c_str()) ? 1 : 0;
    }
    Serial.printf("PocketBGG: covers: %d fetched, %d failed, %u old ones deleted, in %u s\n", coversFetched_, coversFailed_,
                  deleted, static_cast<unsigned>((millis() - coversStartMs_) / 1000));
    finishSync(Sync::Synced);
  }

  // A game's cover from the card, when its details open: a few KB, already dithered.
  void loadCover(const bgg::Game& game) {
    picture_ = bgg::Picture();
    const uint32_t pic = bgg::picId(game.thumbnail);
    if (pic == 0 || !storage::ready()) return;
    size_t size = 0;
    const storage::Blob blob = storage::loadBlob(bgg::thumbName(game.id, pic).c_str(), size);
    if (blob && !bgg::unpackPicture(blob.get(), size, picture_)) {
      Serial.printf("PocketBGG: ignoring game %u's saved cover, which couldn't be read\n", static_cast<unsigned>(game.id));
    }
  }

  // A sort by last played has to be redone once the dates change. The list is redone at once;
  // the details or filter screen redo it on leaving, so neither is pulled away under the player.
  void refreshOrder() {
    if (filter_.order != bgg::Order::LastPlayed) return;
    if (view_ == View::List) {
      applyFilter();
    } else {
      needsRefresh_ = true;
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
    // The collection is saved already. The dates read so far are kept and saved, whether every
    // page came or the plays stopped partway, and a failure then says it was the plays.
    if (phase_ == Phase::Plays) {
      saveList();
      refreshOrder();
      if (result != Sync::Synced && result != Sync::Never) result = Sync::PlaysStopped;
    }
    // The dates were saved before the covers began, and each cover as it came.
    if (phase_ == Phase::Covers && result != Sync::Synced && result != Sync::Never) result = Sync::CoversStopped;
    phase_ = Phase::Collection;
    sync_ = result;
  }

  // Leaving while joining or between requests: no download is running, so the radio can go.
  void stopSync() {
    if (sync_ != Sync::Joining && sync_ != Sync::Waiting) return;
    finishSync(Sync::Never);
  }

  // "READING PLAYS, PAGE 12 OF 34", or without the count until the first page says it.
  const char* playsText(char* out, size_t size) const {
    if (playsPages_ == 0) {
      std::snprintf(out, size, "READING PLAYS, PAGE %d", playsPage_);
    } else {
      std::snprintf(out, size, "READING PLAYS, PAGE %d OF %d", playsPage_, playsPages_);
    }
    return out;
  }

  // "SYNCED 20 NOV, 22:53", in the time zone chosen in Settings.
  const char* syncedText(char* out, size_t size) const {
    if (syncedAt_ == 0) return "SYNCED";
    static const char* const MONTHS[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    const time_t when = static_cast<time_t>(syncedAt_);
    tm local{};
    localtime_r(&when, &local);
    std::snprintf(out, size, "SYNCED %d %s, %02d:%02d", local.tm_mday, MONTHS[local.tm_mon % 12], local.tm_hour, local.tm_min);
    return out;
  }

  const char* statusText(char* out, size_t size) const {
    switch (sync_) {
      case Sync::Never:
        return synced_ ? syncedText(out, size) : "NOT SYNCED YET";
      case Sync::Joining:
        return "JOINING WI-FI";
      case Sync::ConfigProblem:
        return bgg::configErrorText(configError_);
      case Sync::Downloading:
        if (phase_ == Phase::Plays) return playsText(out, size);
        if (phase_ == Phase::Covers) {
          std::snprintf(out, size, "FETCHING COVERS, %d OF %u", coverAt_ + 1, static_cast<unsigned>(covers_.size()));
          return out;
        }
        if (received_.load() == 0) {
          std::snprintf(out, size, attempt_ == 1 ? "ASKING BGG FOR YOUR COLLECTION" : "ASKING AGAIN (%d OF %d)", attempt_, MAX_ATTEMPTS);
        } else {
          std::snprintf(out, size, "DOWNLOADING, %u KB", static_cast<unsigned>(received_.load() / 1024));
        }
        return out;
      case Sync::Waiting:
        if (waitingForBgg_) {
          std::snprintf(out, size, "BGG IS PREPARING IT (%d OF %d)", attempt_, MAX_ATTEMPTS);
          return out;
        }
        return playsText(out, size);
      case Sync::Stopping:
        return "STOPPING";
      case Sync::Synced:
        if (coversFailed_ > 0) {
          std::snprintf(out, size, "SYNCED. %d %s COULDN'T BE FETCHED", coversFailed_, coversFailed_ == 1 ? "COVER" : "COVERS");
          return out;
        }
        return syncedText(out, size);
      case Sync::PlaysStopped:
        if (playsPages_ == 0) return "SYNCED, BUT THE PLAYS COULDN'T BE READ";
        std::snprintf(out, size, "SYNCED. PLAYS STOPPED AT PAGE %d OF %d", playsPage_, playsPages_);
        return out;
      case Sync::CoversStopped:
        std::snprintf(out, size, "SYNCED. COVERS STOPPED AT %d OF %u", coverAt_ + 1, static_cast<unsigned>(covers_.size()));
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
    // FILTER is lesser, so outlined, and dimmed with nothing to filter.
    if (games_.empty()) {
      ui::drawDimmedPill(frame, FILTER_LEFT, BUTTON_TOP, BUTTON_WIDTH, "FILTER");
    } else {
      ui::drawPill(frame, FILTER_LEFT, BUTTON_TOP, BUTTON_WIDTH, "FILTER", false);
    }
    if (sync_ == Sync::Joining || sync_ == Sync::Waiting || sync_ == Sync::Downloading || sync_ == Sync::Stopping) {
      ui::drawDimmedPill(frame, SYNC_LEFT, BUTTON_TOP, BUTTON_WIDTH, "SYNCING");
    } else {
      ui::drawPill(frame, SYNC_LEFT, BUTTON_TOP, BUTTON_WIDTH, "SYNC", true);
    }
  }

  TapResult step(int by) {
    if (view_ == View::List) return turnPage(by);
    if (view_ != View::Details) return TapResult::Nothing;
    const int next = bgg::stepRow(selected_, by, static_cast<int>(visible_.size()));
    if (next < 0) return TapResult::Nothing;
    selected_ = next;
    loadCover(games_[visible_[selected_]]);
    return TapResult::Redraw;
  }

  TapResult turnPage(int step) {
    const int next = page_ + step;
    if (next < 0 || next >= pageCount()) return TapResult::Nothing;
    page_ = next;
    return TapResult::Redraw;
  }

  // How many games, "140 OF 203 GAMES" when the filter hides some, on the left, and on the
  // right which page this is, or on a game's details which game.
  void drawCaption(uint8_t* frame, bool withPage) const {
    char count[32];
    const unsigned all = static_cast<unsigned>(games_.size());
    const unsigned shown = static_cast<unsigned>(visible_.size());
    if (shown == all) {
      std::snprintf(count, sizeof(count), "%u %s", all, all == 1 ? "GAME" : "GAMES");
    } else {
      std::snprintf(count, sizeof(count), "%u OF %u %s", shown, all, all == 1 ? "GAME" : "GAMES");
    }
    ui::drawTextLeft(frame, Font::Size14, MARGIN, CAPTION_TOP, CAPTION_HEIGHT, synced_ ? count : "NOT SYNCED");
    if (visible_.empty()) return;
    char page[24];
    if (withPage) {
      std::snprintf(page, sizeof(page), "PAGE %d OF %d", page_ + 1, pageCount());
    } else {
      std::snprintf(page, sizeof(page), "GAME %d OF %u", selected_ + 1, shown);
    }
    const int width = textWidth(Font::Size14, page);
    ui::drawTextLeft(frame, Font::Size14, MARGIN + INNER_WIDTH - width, CAPTION_TOP, CAPTION_HEIGHT, page);
  }

  void drawList(uint8_t* frame) const {
    drawCaption(frame, true);
    if (visible_.empty()) {
      // Before the first sync, what to do; after one, that BGG had nothing, or the filter hides
      // everything.
      const char* text = !synced_         ? "NO GAMES YET.\n\nPUT CONFIG.TXT IN /POCKETBGG ON THE CARD, THEN TAP SYNC."
                         : games_.empty() ? "YOUR BGG COLLECTION IS EMPTY."
                                          : "NO GAMES MATCH THESE FILTERS.\n\nTAP FILTER TO CHANGE THEM.";
      const int height = wrappedHeight(Font::Size20, INNER_WIDTH, text);
      drawWrapped(frame, Font::Size20, MARGIN, LIST_TOP + (LIST_ROOM - height) / 2, INNER_WIDTH, text, Align::Center);
    }
    for (int row = firstRow(); row < endRow(); row++) {
      const bgg::Game& game = games_[visible_[row]];
      const int top = rowTop(row);
      drawRectOutline(frame, MARGIN, top, INNER_WIDTH, heights_[row], ui::HAIRLINE);
      const int nameHeight = drawWrapped(frame, NAME_FONT, MARGIN + ROW_INSET, top + ROW_PAD_TOP, NAME_WIDTH, game.name.c_str());
      char meta[64];
      bgg::describe(game, meta, sizeof(meta), false);  // the year is on the details screen
      const int metaTop = top + ROW_PAD_TOP + nameHeight;
      ui::drawTextLeft(frame, META_FONT, MARGIN + ROW_INSET, metaTop, META_HEIGHT, meta);
      drawRowDate(frame, game.lastPlayed, textWidth(META_FONT, meta), metaTop);
    }
    drawActions(frame);
  }

  // The last-played date at the right end of a row's line, in the smaller cut: "LAST 17 NOV
  // 2026", then shorter forms if the line leaves too little room, and nothing rather than
  // touching it.
  static void drawRowDate(uint8_t* frame, uint32_t lastPlayed, int metaWidth, int top) {
    if (lastPlayed == 0) return;
    char day[16], month[16], text[24];
    bgg::formatDate(lastPlayed, false, day, sizeof(day));
    bgg::formatDate(lastPlayed, true, month, sizeof(month));
    const char* forms[] = {"LAST %s", "%s"};
    const char* dates[] = {day, month};
    const int room = NAME_WIDTH - metaWidth - GAP;
    for (const char* date : dates) {
      for (const char* form : forms) {
        std::snprintf(text, sizeof(text), form, date);
        const int width = textWidth(Font::Size10, text);
        if (width <= room) {
          ui::drawTextLeft(frame, Font::Size10, MARGIN + ROW_INSET + NAME_WIDTH - width, top, META_HEIGHT, text);
          return;
        }
      }
    }
  }

  void drawDetails(uint8_t* frame, const bgg::Game& game) const {
    drawCaption(frame, false);
    if (!picture_.empty()) {
      // In the middle of the box, with a thin frame so a white cover still shows its edge.
      const int left = THUMB_LEFT + (THUMB - picture_.width) / 2;
      const int top = THUMB_TOP + (THUMB - picture_.height) / 2;
      drawRectOutline(frame, left - 2, top - 2, picture_.width + 4, picture_.height + 4, 1);
      drawIcon(frame, picture_.bits.data(), picture_.width, picture_.height, left, top);
    } else {
      // Not fetched yet, or BGG has no picture of it.
      ui::drawDimmedPill(frame, THUMB_LEFT, THUMB_TOP, THUMB, "", THUMB);
      drawTextInBox(frame, Font::Size14, THUMB_LEFT, THUMB_TOP, THUMB, THUMB,
                    bgg::picId(game.thumbnail) != 0 ? "NO COVER YET" : "NO COVER ON BGG");
    }

    // The name as large as fits in three lines, never cut short.
    Font font = Font::Size30;
    if (wrappedHeight(font, INNER_WIDTH, game.name.c_str()) > 3 * lineHeight(font)) font = Font::Size20;
    const int nameHeight = drawWrapped(frame, font, MARGIN, DETAIL_NAME_TOP, INNER_WIDTH, game.name.c_str(), Align::Center);

    char meta[64];
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
    // When it was last played; nothing for a game with plays but no dated one yet.
    char date[16];
    bgg::formatDate(game.lastPlayed, false, date, sizeof(date));
    if (game.lastPlayed != 0 || game.plays == 0) {
      if (game.lastPlayed != 0) {
        std::snprintf(line, sizeof(line), "LAST PLAYED %s", date);
      } else {
        std::snprintf(line, sizeof(line), "NEVER PLAYED");
      }
      drawTextInBox(frame, Font::Size14, MARGIN, lineTop, INNER_WIDTH, lineHeight(Font::Size14), line);
      lineTop += lineHeight(Font::Size14);
    }
    std::snprintf(line, sizeof(line), "BGG ID %u", static_cast<unsigned>(game.id));
    drawTextInBox(frame, Font::Size14, MARGIN, lineTop, INNER_WIDTH, lineHeight(Font::Size14), line);
    lineTop += lineHeight(Font::Size14) + GAP;
    // BGG asks apps that use its data to say so.
    drawTextInBox(frame, Font::Size10, MARGIN, lineTop, INNER_WIDTH, lineHeight(Font::Size10), "POWERED BY BGG");
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

  std::vector<bgg::Game> games_;  // the collection, from the card or the last sync
  bool loaded_ = false;
  bool synced_ = false;      // there's a collection, even an empty one
  uint32_t syncedAt_ = 0;    // when, in seconds since 1970 UTC
  bool saveFailed_ = false;
  bgg::Filter filter_;  // owned only until the player chooses
  bool filterSaveFailed_ = false;
  std::vector<int> visible_;  // positions in games_ that the filter lets through
  View view_ = View::List;
  bool needsRefresh_ = false;  // the order by last played is out of date, to redo on leaving
  std::vector<int> heights_;  // each row's height, measured once
  std::vector<int> pages_;    // the first row of each page
  int page_ = 0;
  int selected_ = -1;  // the row, in visible_, whose details are showing
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
  Phase phase_ = Phase::Collection;
  std::string username_;   // kept for the plays pages; the token is only in authorization_
  bgg::LastPlayed latest_;  // each game's latest play, from the pages read so far
  int playsPage_ = 0;
  int playsPages_ = 0;  // 0 until the first page says how many
  bool waitingForBgg_ = false;  // a wait after 202, not the pause between pages
  bgg::Picture picture_;  // the open game's cover, 1-bit, about 5 KB
  // The covers this sync fetches, and how it's going.
  struct Cover {
    uint32_t gameId;
    uint32_t pic;
    std::string url;
  };
  std::vector<Cover> covers_;
  int coverAt_ = 0;
  int coversFetched_ = 0;
  int coversFailed_ = 0;
  int coversUnanswered_ = 0;  // one after another
  uint32_t coversStartMs_ = 0;
  uint32_t coverStartMs_ = 0;
  uint32_t lastCoverDrawMs_ = 0;
};

}  // namespace

App& pocketBggApp() {
  static PocketBgg app;
  return app;
}
