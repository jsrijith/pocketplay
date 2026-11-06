#include "connections.h"

#include <Arduino.h>

#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#include "connections_icon.h"
#include "connections_import.h"
#include "connections_pack.h"
#include "connections_results.h"
#include "connections_rules.h"
#include "connections_save.h"
#include "connections_screens.h"
#include "core/download.h"
#include "core/network.h"
#include "core/storage.h"
#include "ui/ui.h"

namespace {

namespace screens = connectionsui;

constexpr const char* BOARD_FILE = "connections";
// The pack: a 4-byte length, the index file, then the data file, as connections_pack.h
// lays them out. One blob, so a power cut can't leave a new index over old data.
constexpr const char* PACK_BLOB = "connections-pack";
// One byte per day since the first puzzle, in connections_results.h's encoding.
constexpr const char* RESULTS_BLOB = "connections-results";
constexpr int MAX_RESULTS = 4096;

// Every published puzzle since 2023 in one file, updated daily. The same source as
// CrossPlay's. The words are the New York Times'; this firmware ships none of them.
constexpr const char* ARCHIVE_URL = "https://raw.githubusercontent.com/Eyefyre/NYT-Connections-Answers/main/connections.json";

// The pack is about 230 KB today. Reserved up front so it grows without copying.
constexpr size_t EXPECTED_INDEX_BYTES = 16 * 1024;
constexpr size_t EXPECTED_DATA_BYTES = 320 * 1024;
// It grows by about 70 KB a year. Far past this, the source has changed.
constexpr size_t MAX_PACK_BYTES = 2 * 1024 * 1024;

constexpr int FIRST_VALID_YEAR = 2023;  // the first puzzle's year
constexpr uint32_t NO_DATE_LIMIT = 99999999;

enum class View { Menu, Board, Archive, HowTo, Importing };
enum class ImportStep { Joining, Downloading, Saving, Done, Failed, Cancelling };

// A read from a pack held in memory.
struct Span {
  const uint8_t* data = nullptr;
  uint32_t size = 0;
};

bool readSpan(void* context, uint32_t offset, void* out, uint32_t size) {
  const auto* span = static_cast<const Span*>(context);
  if (static_cast<uint64_t>(offset) + size > span->size) return false;
  memcpy(out, span->data + offset, size);
  return true;
}

// --- The download. These run in the download task until it ends, and touch memory
// and nothing else. ---

std::vector<uint8_t> downloadIndex;
std::vector<uint8_t> downloadData;
connections::Importer importer;
connections::PackWriter writer;
bool tooLarge = false;

bool appendTo(void* context, const void* data, uint32_t size) {
  auto* out = static_cast<std::vector<uint8_t>*>(context);
  if (out->size() + size > MAX_PACK_BYTES) {
    tooLarge = true;
    return false;
  }
  const auto* bytes = static_cast<const uint8_t*>(data);
  out->insert(out->end(), bytes, bytes + size);
  return true;
}

// A puzzle the writer refuses (out of order, unplayable) is skipped, not fatal.
bool addPuzzle(void*, const connections::Puzzle& puzzle) {
  static_cast<void>(writer.add(puzzle));
  return !tooLarge;
}

void beginParsing() {
  downloadIndex.clear();
  downloadData.clear();
  tooLarge = false;
  importer.begin(addPuzzle, nullptr);
  writer.begin(appendTo, &downloadIndex, &downloadData);
}

bool writeDownload(void*, const uint8_t* data, size_t size) { return importer.feed(data, size); }

// The download started again from the top, so everything so far is thrown away.
void restartDownload(void*) { beginParsing(); }

void freeDownload() {
  std::vector<uint8_t>().swap(downloadIndex);
  std::vector<uint8_t>().swap(downloadData);
}

class Connections final : public Game {
 public:
  const char* name() const override { return "CONNECTIONS"; }
  const uint8_t* icon() const override { return connectionsIcon::ICON; }

  // Always the menu, as in CrossPlay: it's the one screen that says what there is to
  // do. A puzzle left half done is still loaded, and the menu's TODAY or the archive
  // picks it up where it was.
  void open() override {
    if (!loaded_) {
      loaded_ = true;
      loadPack();
      loadResults();
      loadBoard();
    }
    view_ = View::Menu;
  }

  TapResult onTap(int x, int y) override {
    if (ui::isOnBack(x, y)) return tapBack();
    switch (view_) {
      case View::Menu:
        return tapMenu(x, y);
      case View::Board:
        return tapBoard(x, y);
      case View::Archive:
        return tapArchive(x, y);
      case View::HowTo:
        // Tap anywhere to leave it, as in CrossPlay.
        return showMenu();
      case View::Importing:
        return importStep_ == ImportStep::Done || importStep_ == ImportStep::Failed ? showMenu() : TapResult::Nothing;
    }
    return TapResult::Nothing;
  }

  void draw(uint8_t* frame) override {
    switch (view_) {
      case View::Menu:
        drawMenu(frame);
        break;
      case View::Board:
        drawBoard(frame);
        break;
      case View::Archive:
        drawArchive(frame);
        break;
      case View::HowTo:
        screens::drawHowTo(frame);
        break;
      case View::Importing:
        drawImport(frame);
        break;
    }
  }

  bool saveFailed() const override { return saveFailed_; }

  void retrySave() override {
    saveFailed_ = false;
    persist();
    if (resultsDirty_) saveResults();
  }

  bool update() override { return view_ == View::Importing && updateImport(); }

  bool busy() const override { return view_ == View::Importing && (importStep_ == ImportStep::Downloading || importStep_ == ImportStep::Saving || importStep_ == ImportStep::Cancelling); }

  void onWifiClosed() override {
    if (view_ != View::Importing) return;
    if (network::state() == network::State::Connected) {
      startDownload();
    } else {
      // BACK during a join leaves the radio still trying.
      finishImport(ImportStep::Failed, "NO WI-FI");
    }
  }

 private:
  // --- Dates ---

  // Today as YYYYMMDD, or 0 with no clock.
  static uint32_t today() {
    const time_t now = time(nullptr);
    tm parts;
    localtime_r(&now, &parts);
    if (parts.tm_year + 1900 < FIRST_VALID_YEAR) return 0;
    return static_cast<uint32_t>((parts.tm_year + 1900) * 10000 + (parts.tm_mon + 1) * 100 + parts.tm_mday);
  }

  // With no clock, every puzzle on the card counts as already published.
  static uint32_t todayOrLimit() {
    const uint32_t now = today();
    return now != 0 ? now : NO_DATE_LIMIT;
  }

  bool hasPuzzles() const { return pack_.isOpen() && pack_.count() > 0; }

  // --- The pack and the record ---

  void loadPack() {
    pack_ = connections::PackReader();
    packBlob_.reset();
    firstDate_ = 0;
    lastDate_ = 0;

    size_t size = 0;
    storage::Blob blob = storage::loadBlob(PACK_BLOB, size);
    if (!blob || size < 4) return;
    const uint32_t indexSize = static_cast<uint32_t>(blob[0]) | (static_cast<uint32_t>(blob[1]) << 8) | (static_cast<uint32_t>(blob[2]) << 16) | (static_cast<uint32_t>(blob[3]) << 24);
    if (indexSize > size - 4) return;
    indexSpan_ = Span{blob.get() + 4, indexSize};
    dataSpan_ = Span{blob.get() + 4 + indexSize, static_cast<uint32_t>(size - 4 - indexSize)};
    if (!pack_.open(readSpan, &indexSpan_, &dataSpan_, indexSize)) {
      Serial.println("Ignoring the saved Connections pack");
      pack_ = connections::PackReader();
      return;
    }
    packBlob_ = std::move(blob);
    pack_.dateAt(0, firstDate_);
    pack_.dateAt(pack_.count() - 1, lastDate_);
    Serial.printf("Connections pack: %d puzzles, %u to %u\n", pack_.count(), static_cast<unsigned>(firstDate_), static_cast<unsigned>(lastDate_));
  }

  void loadResults() {
    results_.clear();
    size_t size = 0;
    const storage::Blob blob = storage::loadBlob(RESULTS_BLOB, size);
    if (!blob) return;
    if (size > MAX_RESULTS) size = MAX_RESULTS;
    results_.assign(blob.get(), blob.get() + size);
  }

  connections::Record recordFor(uint32_t date) const {
    const int at = connections::resultIndex(date);
    if (at < 0 || at >= static_cast<int>(results_.size())) return connections::Record{};
    return connections::decodeRecord(results_[at]);
  }

  void noteSave(bool succeeded) {
    if (storage::ready() && !succeeded) saveFailed_ = true;
  }

  void saveResults() {
    resultsDirty_ = !storage::saveBlob(RESULTS_BLOB, results_.data(), results_.size());
    noteSave(!resultsDirty_);
  }

  // The first finish of a day is the one kept: a replay never rewrites it.
  void recordFinish() {
    const uint32_t date = game_.puzzle().date;
    const int at = connections::resultIndex(date);
    if (at < 0 || at >= MAX_RESULTS || recordFor(date).played) return;

    connections::Record record;
    record.played = true;
    record.lost = game_.result() == connections::Result::Lost;
    record.mistakes = static_cast<uint8_t>(game_.mistakes());
    // Only claimable where the source published colour data. solvedGroup(0) is the
    // group found first.
    record.hardestFirst = !record.lost && game_.solvedCount() > 0 && game_.solvedGroup(0).level == 3;
    if (static_cast<int>(results_.size()) <= at) results_.resize(static_cast<size_t>(at) + 1, 0);
    results_[at] = connections::encodeRecord(record);
    saveResults();
  }

  // --- The board in hand ---

  // Which of the puzzle's sixteen words a tile holds, as group * 4 + member. Words are
  // unique on a board (isPlayable), so the word alone says which.
  int wordBit(int tile) const {
    const char* word = game_.tileWord(tile);
    const connections::Puzzle& puzzle = game_.puzzle();
    for (int g = 0; g < connections::kGroups; g++) {
      for (int m = 0; m < connections::kMembers; m++) {
        if (strcmp(puzzle.groups[g].members[m], word) == 0) return g * connections::kMembers + m;
      }
    }
    return 0;
  }

  // Only a game still being played has a save, or it would be offered for resume.
  void persist() {
    if (!storage::ready()) return;
    if (puzzleIndex_ < 0 || game_.result() != connections::Result::Playing) {
      noteSave(storage::erase(BOARD_FILE));
      return;
    }
    connections::OpenBoard board;
    board.index = puzzleIndex_;
    board.date = game_.puzzle().date;
    board.state = game_.save();
    for (int i = 0; i < game_.tileCount(); i++) {
      if (game_.isSelected(i)) board.selected = static_cast<uint16_t>(board.selected | (1u << wordBit(i)));
    }
    uint8_t data[connections::SAVE_BYTES];
    const size_t size = connections::packBoard(board, data);
    const uint32_t startUs = micros();
    noteSave(storage::save(BOARD_FILE, connections::SAVE_VERSION, data, size));
    Serial.printf("Saved %s in %lu us\n", BOARD_FILE, static_cast<unsigned long>(micros() - startUs));
  }

  void loadBoard() {
    if (!storage::ready() || !hasPuzzles()) return;
    uint8_t data[connections::SAVE_BYTES];
    uint8_t version = 0;
    size_t size = 0;
    if (!storage::load(BOARD_FILE, version, data, sizeof(data), size)) return;

    connections::OpenBoard board;
    if (!connections::unpackBoard(version, data, size, board)) {
      Serial.println("Ignoring saved Connections board");
      return;
    }
    // A pack downloaded since may hold the puzzle at another position, so the date
    // decides which it is.
    connections::Puzzle puzzle;
    if (!pack_.readPuzzle(board.index, puzzle) || puzzle.date != board.date) {
      board.index = pack_.indexOfDate(board.date);
      if (board.index < 0 || !pack_.readPuzzle(board.index, puzzle)) {
        Serial.println("Ignoring saved Connections board: its puzzle isn't in the pack");
        return;
      }
    }
    game_.start(puzzle, board.state.seed);
    if (!game_.restore(board.state)) {
      Serial.println("Ignoring saved Connections board");
      game_ = connections::Game();
      return;
    }
    for (int i = 0; i < game_.tileCount(); i++) {
      if (board.selected & (1u << wordBit(i))) game_.toggleTile(i);
    }
    puzzleIndex_ = board.index;
  }

  // A board for the puzzle at index. The one already being played is picked up where
  // it was; any other starts fresh, including a finished one played again.
  bool openPuzzle(int index) {
    if (index < 0) return false;
    if (index == puzzleIndex_ && game_.result() == connections::Result::Playing) {
      toast_ = nullptr;
      view_ = View::Board;
      return true;
    }
    connections::Puzzle puzzle;
    if (!hasPuzzles() || !pack_.readPuzzle(index, puzzle)) return false;
    // Seeded from the puzzle's id, so the same puzzle always deals the same board.
    game_.start(puzzle, 0x9E3779B9u ^ (static_cast<uint32_t>(puzzle.id) * 2654435761u));
    puzzleIndex_ = index;
    toast_ = nullptr;
    view_ = View::Board;
    persist();
    Serial.printf("Connections %u opened\n", static_cast<unsigned>(puzzle.date));
    return true;
  }

  // --- Moving between views ---

  TapResult showMenu() {
    view_ = View::Menu;
    return TapResult::Redraw;
  }

  TapResult tapBack() {
    switch (view_) {
      case View::Menu:
        return TapResult::Leave;
      case View::Board:
      case View::Archive:
      case View::HowTo:
        return showMenu();
      case View::Importing:
        return backFromImport();
    }
    return TapResult::Nothing;
  }

  TapResult tapMenu(int x, int y) {
    switch (screens::menuTapAt(x, y)) {
      case screens::MenuTap::Nothing:
        return TapResult::Nothing;
      case screens::MenuTap::Today:
        return hasPuzzles() && openPuzzle(pack_.indexOnOrBefore(todayOrLimit())) ? TapResult::RedrawFull : TapResult::Nothing;
      case screens::MenuTap::Archive:
        if (!hasPuzzles()) return TapResult::Nothing;
        showMonthOf(today() != 0 && today() <= lastDate_ ? today() : lastDate_);
        view_ = View::Archive;
        return TapResult::Redraw;
      case screens::MenuTap::HowTo:
        view_ = View::HowTo;
        return TapResult::Redraw;
      case screens::MenuTap::GetPuzzles:
        return upToDate() ? TapResult::Nothing : beginImport();
    }
    return TapResult::Nothing;
  }

  // The archive publishes daily, so holding today's puzzle means holding them all.
  bool upToDate() const { return hasPuzzles() && today() != 0 && lastDate_ >= today(); }

  TapResult tapBoard(int x, int y) {
    int tile = -1;
    const bool hadToast = toast_ != nullptr;
    switch (screens::boardTapAt(game_, x, y, tile)) {
      case screens::BoardTap::Nothing:
        return TapResult::Nothing;
      case screens::BoardTap::Done:
        return showMenu();
      case screens::BoardTap::Tile: {
        const bool wasSelected = game_.isSelected(tile);
        const int before = game_.selectedCount();
        game_.toggleTile(tile);
        toast_ = nullptr;
        // A fifth tile is refused, so nothing changes.
        if (game_.selectedCount() == before && game_.isSelected(tile) == wasSelected) return hadToast ? TapResult::Redraw : TapResult::Nothing;
        persist();
        return TapResult::Redraw;
      }
      case screens::BoardTap::Shuffle:
        toast_ = nullptr;
        game_.shuffle(game_.seed() * 1103515245u + 12345u);
        persist();
        return TapResult::Redraw;
      case screens::BoardTap::Clear:
        if (game_.selectedCount() == 0) {
          toast_ = nullptr;
          return hadToast ? TapResult::Redraw : TapResult::Nothing;
        }
        toast_ = nullptr;
        game_.deselectAll();
        persist();
        return TapResult::Redraw;
      case screens::BoardTap::Submit:
        return submit();
    }
    return TapResult::Nothing;
  }

  TapResult submit() {
    if (!game_.canSubmit()) return TapResult::Nothing;
    switch (game_.submit()) {
      case connections::Guess::Invalid:
        return TapResult::Nothing;
      case connections::Guess::AlreadyGuessed:
        // Costs nothing and changes nothing but the message.
        if (toast_ != nullptr && strcmp(toast_, "ALREADY GUESSED") == 0) return TapResult::Nothing;
        toast_ = "ALREADY GUESSED";
        return TapResult::Redraw;
      case connections::Guess::OneAway:
        toast_ = "ONE AWAY";
        break;
      case connections::Guess::Wrong:
        toast_ = "NOT A GROUP";
        break;
      case connections::Guess::Solved:
        toast_ = nullptr;
        break;
    }
    if (game_.result() == connections::Result::Playing) {
      persist();
      return TapResult::Redraw;
    }
    recordFinish();
    persist();  // a finished game has no save
    Serial.printf("Connections %u %s with %d mistakes\n", static_cast<unsigned>(game_.puzzle().date), game_.result() == connections::Result::Won ? "won" : "lost", game_.mistakes());
    return TapResult::RedrawFull;
  }

  // --- The archive ---

  void showMonthOf(uint32_t date) {
    if (date == 0) return;
    calendarYear_ = static_cast<int>(date / 10000);
    calendarMonth_ = static_cast<int>(date / 100 % 100);
    buildCalendar();
  }

  uint32_t dateForCell(int cell) const {
    if (cell < 0 || cell >= screens::CALENDAR_CELLS) return 0;
    const screens::CalendarDay& day = calendarCells_[cell];
    if (day.day == 0 || !day.inArchive) return 0;
    return static_cast<uint32_t>(calendarYear_) * 10000u + static_cast<uint32_t>(calendarMonth_) * 100u + day.day;
  }

  void buildCalendar() {
    for (auto& cell : calendarCells_) cell = screens::CalendarDay{};
    calendarPlayed_ = 0;
    const uint32_t firstOfMonth = static_cast<uint32_t>(calendarYear_) * 10000u + static_cast<uint32_t>(calendarMonth_) * 100u + 1u;
    const int lead = connections::dayOfWeek(firstOfMonth);
    const int days = connections::daysInMonth(calendarYear_, calendarMonth_);
    if (lead < 0 || days == 0) return;

    for (int d = 1; d <= days; d++) {
      const int i = lead + d - 1;
      if (i >= screens::CALENDAR_CELLS) break;
      const uint32_t date = firstOfMonth + static_cast<uint32_t>(d - 1);
      screens::CalendarDay& cell = calendarCells_[i];
      cell.day = static_cast<uint8_t>(d);
      // In the archive when the pack holds it, not merely when it's in range: two dates
      // are missing on purpose (emoji puzzles).
      cell.inArchive = pack_.indexOfDate(date) >= 0;
      if (!cell.inArchive) continue;
      const connections::Record record = recordFor(date);
      cell.played = record.played;
      cell.lost = record.lost;
      cell.mistakes = record.mistakes;
      if (record.played) calendarPlayed_++;
    }
  }

  static void addMonths(int& year, int& month, int months) {
    month += months;
    while (month < 1) {
      month += 12;
      year--;
    }
    while (month > 12) {
      month -= 12;
      year++;
    }
  }

  // A month is reachable when any of its days is in the archive, so stepping stops at
  // the edges instead of opening a month of greyed numbers.
  bool canStep(int months) const {
    int year = calendarYear_;
    int month = calendarMonth_;
    addMonths(year, month, months);
    const uint32_t firstOfMonth = static_cast<uint32_t>(year) * 10000u + static_cast<uint32_t>(month) * 100u + 1u;
    const uint32_t lastOfMonth = firstOfMonth + static_cast<uint32_t>(connections::daysInMonth(year, month)) - 1u;
    if (firstDate_ != 0 && lastOfMonth < firstDate_) return false;
    if (lastDate_ != 0 && firstOfMonth > lastDate_) return false;
    return true;
  }

  // CrossPlay's year arrows move by whole years but stop at the archive's first and
  // last years, not its first and last months.
  bool canStepYear(int years) const {
    const int year = calendarYear_ + years;
    if (firstDate_ != 0 && year < static_cast<int>(firstDate_ / 10000)) return false;
    if (lastDate_ != 0 && year > static_cast<int>(lastDate_ / 10000)) return false;
    return true;
  }

  TapResult stepMonths(int months) {
    if (!canStep(months)) return TapResult::Nothing;
    addMonths(calendarYear_, calendarMonth_, months);
    buildCalendar();
    return TapResult::Redraw;
  }

  TapResult stepYears(int years) {
    if (!canStepYear(years)) return TapResult::Nothing;
    calendarYear_ += years;
    buildCalendar();
    return TapResult::Redraw;
  }

  TapResult tapArchive(int x, int y) {
    int cell = -1;
    switch (screens::calendarTapAt(x, y, cell)) {
      case screens::CalendarTap::Nothing:
        return TapResult::Nothing;
      case screens::CalendarTap::Today: {
        const uint32_t date = today() != 0 && today() <= lastDate_ ? today() : lastDate_;
        if (static_cast<int>(date / 10000) == calendarYear_ && static_cast<int>(date / 100 % 100) == calendarMonth_) return TapResult::Nothing;
        showMonthOf(date);
        return TapResult::Redraw;
      }
      case screens::CalendarTap::PrevYear:
        return stepYears(-1);
      case screens::CalendarTap::NextYear:
        return stepYears(1);
      case screens::CalendarTap::PrevMonth:
        return stepMonths(-1);
      case screens::CalendarTap::NextMonth:
        return stepMonths(1);
      case screens::CalendarTap::Day: {
        const uint32_t date = dateForCell(cell);
        if (date == 0) return TapResult::Nothing;
        return openPuzzle(pack_.indexOfDate(date)) ? TapResult::RedrawFull : TapResult::Nothing;
      }
    }
    return TapResult::Nothing;
  }

  // --- Getting the puzzles ---

  TapResult beginImport() {
    view_ = View::Importing;
    imported_ = 0;
    if (network::state() == network::State::Connected) {
      startDownload();
      return TapResult::Redraw;
    }
    importStep_ = ImportStep::Joining;
    importDetail_ = "JOINING WI-FI";
    return TapResult::OpenWifi;
  }

  void startDownload() {
    beginParsing();
    downloadIndex.reserve(EXPECTED_INDEX_BYTES);
    downloadData.reserve(EXPECTED_DATA_BYTES);
    if (!download::start(ARCHIVE_URL, download::Sink{nullptr, writeDownload, restartDownload})) {
      freeDownload();
      finishImport(ImportStep::Failed, "DOWNLOAD FAILED");
      return;
    }
    importStep_ = ImportStep::Downloading;
    importDetail_ = "DOWNLOADING";
  }

  TapResult backFromImport() {
    switch (importStep_) {
      case ImportStep::Downloading:
        download::requestCancel();
        importStep_ = ImportStep::Cancelling;
        importDetail_ = "STOPPING";
        return TapResult::Redraw;
      case ImportStep::Cancelling:
      case ImportStep::Saving:
        return TapResult::Nothing;
      case ImportStep::Joining:
      case ImportStep::Done:
      case ImportStep::Failed:
        return showMenu();
    }
    return TapResult::Nothing;
  }

  // The radio stays on only as long as it's being used.
  void finishImport(ImportStep step, const char* detail) {
    importStep_ = step;
    importDetail_ = detail;
    network::turnOff();
    if (step == ImportStep::Failed) Serial.printf("Connections puzzles: %s, HTTP status %d\n", detail, download::httpStatus());
  }

  bool updateImport() {
    switch (importStep_) {
      case ImportStep::Downloading: {
        const download::State state = download::state();
        if (state == download::State::Running) return false;
        if (state != download::State::Done) {
          freeDownload();
          finishImport(ImportStep::Failed, "DOWNLOAD FAILED");
          return true;
        }
        const bool parsed = importer.finish();
        const connections::ImportStats stats = importer.stats();
        Serial.printf("Connections import: %d puzzles, %d unrenderable, %d malformed\n", stats.accepted, stats.skippedUnrenderable, stats.skippedMalformed);
        if (tooLarge || !parsed || writer.written() == 0) {
          freeDownload();
          finishImport(ImportStep::Failed, tooLarge ? "TOO MANY PUZZLES" : "BAD DATA");
          return true;
        }
        // Drawn before the card write, which takes a moment.
        importStep_ = ImportStep::Saving;
        importDetail_ = "SAVING";
        return true;
      }

      case ImportStep::Saving:
        saveImport();
        return true;

      case ImportStep::Cancelling:
        if (download::state() == download::State::Running) return false;
        freeDownload();
        network::turnOff();
        return showMenu() == TapResult::Redraw;

      case ImportStep::Joining:
      case ImportStep::Done:
      case ImportStep::Failed:
        return false;
    }
    return false;
  }

  // On this side of the download task, because the card shares the panel's bus.
  void saveImport() {
    const uint32_t indexSize = static_cast<uint32_t>(downloadIndex.size());
    std::vector<uint8_t> blob;
    blob.reserve(4 + downloadIndex.size() + downloadData.size());
    for (int shift = 0; shift < 32; shift += 8) blob.push_back(static_cast<uint8_t>(indexSize >> shift));
    blob.insert(blob.end(), downloadIndex.begin(), downloadIndex.end());
    blob.insert(blob.end(), downloadData.begin(), downloadData.end());
    freeDownload();

    const uint32_t heldDate = game_.puzzle().date;
    if (!storage::saveBlob(PACK_BLOB, blob.data(), blob.size())) {
      finishImport(ImportStep::Failed, "CANNOT WRITE TO CARD");
      return;
    }
    std::vector<uint8_t>().swap(blob);
    loadPack();

    // The new pack may hold the puzzle in hand at another position.
    if (puzzleIndex_ >= 0) {
      puzzleIndex_ = pack_.indexOfDate(heldDate);
      if (puzzleIndex_ < 0) game_ = connections::Game();
      persist();
    }
    imported_ = pack_.count();
    finishImport(hasPuzzles() ? ImportStep::Done : ImportStep::Failed, hasPuzzles() ? "TAP TO PLAY" : "BAD DATA");
  }

  // --- Drawing ---

  void drawMenu(uint8_t* frame) const {
    screens::MenuModel model;
    model.hasPuzzles = hasPuzzles();
    if (model.hasPuzzles) {
      model.puzzleCount = pack_.count();
      model.newestDate = lastDate_;
      model.upToDate = upToDate();
      const connections::Summary summary = connections::summarise(results_.data(), static_cast<int>(results_.size()), connections::resultIndex(lastDate_));
      model.played = summary.played;
      model.perfect = summary.perfect;
      model.streak = summary.streak;
      model.todayDone = summary.newestPlayed;
      for (int i = 0; i < 16; i++) model.recent[i] = static_cast<screens::DayResult>(summary.recent[i]);
      // A puzzle loaded but not yet recorded is one walked away from.
      model.inProgress = puzzleIndex_ >= 0 && game_.puzzle().date == lastDate_ && !summary.newestPlayed;
    }
    screens::drawMenu(frame, model);
  }

  void drawBoard(uint8_t* frame) const {
    screens::BoardModel model;
    model.game = &game_;
    model.date = game_.puzzle().date;
    model.toast = toast_;
    screens::drawBoard(frame, model);
  }

  void drawArchive(uint8_t* frame) const {
    screens::CalendarModel model;
    model.year = calendarYear_;
    model.month = calendarMonth_;
    model.playedThisMonth = calendarPlayed_;
    model.cells = calendarCells_;
    model.canPrevYear = canStepYear(-1);
    model.canNextYear = canStepYear(1);
    model.canPrevMonth = canStep(-1);
    model.canNextMonth = canStep(1);
    const uint32_t now = today();
    if (static_cast<int>(now / 10000) == calendarYear_ && static_cast<int>(now / 100 % 100) == calendarMonth_) {
      const int lead = connections::dayOfWeek(static_cast<uint32_t>(calendarYear_) * 10000u + static_cast<uint32_t>(calendarMonth_) * 100u + 1u);
      if (lead >= 0) model.todayCell = lead + static_cast<int>(now % 100) - 1;
    }
    screens::drawCalendar(frame, model);
  }

  void drawImport(uint8_t* frame) const {
    screens::ImportModel model;
    model.detail = importDetail_;
    model.puzzles = imported_;
    model.done = importStep_ == ImportStep::Done;
    model.failed = importStep_ == ImportStep::Failed;
    screens::drawImport(frame, model);
  }

  bool loaded_ = false;
  View view_ = View::Menu;

  storage::Blob packBlob_;
  Span indexSpan_;
  Span dataSpan_;
  connections::PackReader pack_;
  uint32_t firstDate_ = 0;
  uint32_t lastDate_ = 0;

  std::vector<uint8_t> results_;
  bool resultsDirty_ = false;

  connections::Game game_;
  int puzzleIndex_ = -1;  // the puzzle in game_, or -1 for none
  const char* toast_ = nullptr;

  screens::CalendarDay calendarCells_[screens::CALENDAR_CELLS] = {};
  int calendarYear_ = 2026;
  int calendarMonth_ = 1;
  int calendarPlayed_ = 0;

  ImportStep importStep_ = ImportStep::Joining;
  const char* importDetail_ = "";
  int imported_ = 0;

  bool saveFailed_ = false;
};

}  // namespace

Game& connectionsGame() {
  static Connections game;
  return game;
}
