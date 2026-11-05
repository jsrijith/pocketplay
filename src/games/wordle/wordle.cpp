#include "wordle.h"

#include <Arduino.h>

#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <utility>

#include "core/network.h"
#include "core/storage.h"
#include "ui/ui.h"
#include "wordle_import.h"
#include "wordle_rules.h"
#include "wordle_save.h"
#include "wordle_screens.h"

namespace {

namespace screens = wordleui;

// The game on screen, in a save. Whichever day is open also has its guesses in the
// games blob, which is kept for every day played.
constexpr const char* OPEN_GAME_FILE = "wordle";
constexpr const char* ANSWERS_BLOB = "wordle-answers";
constexpr const char* WORDS_BLOB = "wordle-words";
constexpr const char* RESULTS_BLOB = "wordle-results";
constexpr const char* GAMES_BLOB = "wordle-games";

constexpr const char* MONTHS[12] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

constexpr int FIRST_VALID_YEAR = 2021;  // the first puzzle's year

enum class View { Menu, Game, Archive, HowTo, Importing };
enum class ImportStep { Joining, WaitingForTime, Downloading, Saving, Done, Failed, Cancelling };

bool readBlob(const char* name, std::string& out) {
  out.clear();
  size_t size = 0;
  const storage::Blob blob = storage::loadBlob(name, size);
  if (!blob) return false;
  out.assign(reinterpret_cast<const char*>(blob.get()), size);
  return true;
}

bool writeBlob(const char* name, const std::string& data) { return storage::saveBlob(name, reinterpret_cast<const uint8_t*>(data.data()), data.size()); }

void formatDay(int day, bool withYear, char* out, size_t size) {
  int year = 0;
  int month = 0;
  int date = 0;
  wordle::dateOfDay(day, year, month, date);
  if (withYear) {
    snprintf(out, size, "%d %s %d", date, MONTHS[month - 1], year);
  } else {
    snprintf(out, size, "%s %d", MONTHS[month - 1], date);
  }
}

bool isFinishedResult(uint8_t result) {
  const uint8_t value = result & 0x7F;
  return (value >= 1 && value <= wordle::kRows) || value == wordle::kLost;
}

// Day 0, 2021-06-19, was a Saturday, and cells run Sunday first.
int leadingBlanks(int firstOfMonth) { return ((firstOfMonth % 7) + 7 + 6) % 7; }

int firstOfNextMonth(int year, int month) { return month == 12 ? wordle::dayIndex(year + 1, 1, 1) : wordle::dayIndex(year, month + 1, 1); }

bool isListed(void* context, const char* word) {
  const auto* list = static_cast<std::pair<const char*, size_t>*>(context);
  return wordle::isWord(list->first, list->second, word);
}

class WordleGame final : public Game {
 public:
  const char* name() const override { return "WORDLE"; }

  void open() override {
    readBlob(ANSWERS_BLOB, answers_);
    readBlob(RESULTS_BLOB, results_);
    view_ = View::Menu;
    reopenSavedGame();
  }

  TapResult onTap(int x, int y) override {
    if (ui::isOnBack(x, y)) return tapBack();
    switch (view_) {
      case View::Menu:
        return tapMenu(x, y);
      case View::Game:
        return tapGame(x, y);
      case View::Archive:
        return tapArchive(x, y);
      case View::HowTo:
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
      case View::Game:
        drawGame(frame);
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
    if (view_ != View::Game) return;
    saveOpenGame();
    saveGuesses();
    saveResults();
  }

  bool update() override { return view_ == View::Importing && updateImport(); }

  bool busy() const override { return view_ == View::Importing && (importStep_ == ImportStep::WaitingForTime || importStep_ == ImportStep::Downloading || importStep_ == ImportStep::Cancelling); }

  void onWifiClosed() override {
    if (view_ != View::Importing) return;
    if (network::state() == network::State::Connected) {
      importStep_ = ImportStep::WaitingForTime;
      importDetail_ = "CHECKING THE DATE";
    } else {
      // BACK during a join leaves the radio still trying.
      finishImport(ImportStep::Failed, "NO WI-FI");
    }
  }

 private:
  // --- Dates ---

  int today() const {
    const time_t now = time(nullptr);
    tm parts;
    localtime_r(&now, &parts);

    // No clock: the newest puzzle on the card stands in for today.
    if (parts.tm_year + 1900 < FIRST_VALID_YEAR) return wordle::lastKnownDay(answers_);
    return wordle::dayIndex(parts.tm_year + 1900, parts.tm_mon + 1, parts.tm_mday);
  }

  // The puzzle the menu offers: today's, or the newest held when today's isn't on the
  // card yet.
  int newestDay() const {
    const int last = wordle::lastKnownDay(answers_);
    const int now = today();
    return last < now ? last : now;
  }

  // The newest day with an answer on the card that isn't finished yet, or -1.
  int nextUnfinished() const {
    for (int day = newestDay(); day >= 0; day--) {
      char word[wordle::kLetters];
      if (!isFinishedResult(wordle::resultOf(results_, day)) && wordle::answerFor(answers_, day, word)) return day;
    }
    return -1;
  }

  // --- Saves ---

  bool loadWords() {
    if (words_) return true;
    size_t size = 0;
    storage::Blob blob = storage::loadBlob(WORDS_BLOB, size);
    if (!blob || size == 0 || size % wordle::kLetters != 0) return false;
    words_ = std::move(blob);
    wordCount_ = size / wordle::kLetters;
    return true;
  }

  void noteSave(bool succeeded) {
    if (storage::ready() && !succeeded) saveFailed_ = true;
  }

  void saveOpenGame() {
    wordle::OpenGame open;
    open.day = game_.day();
    open.typed = game_.typed();
    memcpy(open.letters, game_.typing(), game_.typed());

    uint8_t data[wordle::SAVE_BYTES];
    const size_t size = wordle::packOpenGame(open, data);
    const uint32_t startUs = micros();
    noteSave(storage::save(OPEN_GAME_FILE, wordle::SAVE_VERSION, data, size));
    Serial.printf("Saved %s in %lu us\n", OPEN_GAME_FILE, static_cast<unsigned long>(micros() - startUs));
  }

  void saveResults() { noteSave(writeBlob(RESULTS_BLOB, results_)); }

  // One line per day played, so a day reopened from the archive shows what was put down.
  void saveGuesses() {
    if (game_.day() < 0) return;

    std::string games;
    readBlob(GAMES_BLOB, games);
    char key[16];
    const int keyLength = snprintf(key, sizeof(key), "%d ", game_.day());

    std::string out;
    out.reserve(games.size() + 48);
    size_t at = 0;
    while (at < games.size()) {
      const size_t end = games.find('\n', at);
      const size_t length = (end == std::string::npos ? games.size() : end) - at;
      const bool isThisDay = length > static_cast<size_t>(keyLength) && games.compare(at, keyLength, key) == 0;
      if (!isThisDay && length > 0) {
        out.append(games, at, length);
        out += '\n';
      }
      if (end == std::string::npos) break;
      at = end + 1;
    }
    if (game_.guesses() > 0) {
      out += key;
      out += game_.save();
      out += '\n';
    }
    noteSave(writeBlob(GAMES_BLOB, out));
  }

  void restoreGuesses(int day, const char* answer) {
    std::string games;
    readBlob(GAMES_BLOB, games);
    char key[16];
    const int keyLength = snprintf(key, sizeof(key), "%d ", day);

    size_t at = 0;
    while (at < games.size()) {
      const size_t end = games.find('\n', at);
      const size_t length = (end == std::string::npos ? games.size() : end) - at;
      if (length > static_cast<size_t>(keyLength) && games.compare(at, keyLength, key) == 0) {
        if (!game_.load(day, games.substr(at + keyLength, length - keyLength))) game_.start(day, answer);
        return;
      }
      if (end == std::string::npos) return;
      at = end + 1;
    }
  }

  // A wake comes back to the game that was on screen, half-typed row and all.
  void reopenSavedGame() {
    if (!storage::ready()) return;

    uint8_t data[wordle::SAVE_BYTES];
    uint8_t version = 0;
    size_t size = 0;
    if (!storage::load(OPEN_GAME_FILE, version, data, sizeof(data), size)) return;

    wordle::OpenGame open;
    if (!wordle::unpackOpenGame(version, data, size, open)) {
      Serial.println("Ignoring saved Wordle game");
      return;
    }
    if (!openDay(open.day)) return;
    for (int i = 0; i < open.typed; i++) {
      game_.type(open.letters[i]);
    }

    // openDay() just saved an empty row over the one being restored.
    if (open.typed > 0) saveOpenGame();
  }

  // --- Moving between views ---

  // Returns false if that day can't be played yet.
  bool openDay(int day) {
    char answer[wordle::kLetters];
    if (!wordle::answerFor(answers_, day, answer)) return false;
    if (!loadWords()) {
      message_ = "Get the puzzles again.";
      return false;
    }

    game_.start(day, answer);
    restoreGuesses(day, answer);
    if (wordle::resultOf(results_, day) == 0) {
      wordle::putResult(results_, day, static_cast<uint8_t>(wordle::kStarted | (day == today() ? wordle::kOnTheDay : 0)));
      saveResults();
    }
    message_ = nullptr;
    view_ = View::Game;
    saveOpenGame();
    Serial.printf("Wordle day %d opened, %d guesses in\n", day, game_.guesses());
    return true;
  }

  TapResult showMenu() {
    view_ = View::Menu;
    return TapResult::Redraw;
  }

  TapResult tapBack() {
    switch (view_) {
      case View::Menu:
        return TapResult::Leave;
      case View::Game:
        noteSave(storage::erase(OPEN_GAME_FILE));
        return showMenu();
      case View::Archive:
      case View::HowTo:
        return showMenu();
      case View::Importing:
        return backFromImport();
    }
    return TapResult::Nothing;
  }

  // --- Taps ---

  TapResult tapMenu(int x, int y) {
    switch (screens::menuTapAt(x, y)) {
      case screens::MenuTap::Nothing:
        return TapResult::Nothing;
      case screens::MenuTap::Today:
        return newestDay() >= 0 && openDay(newestDay()) ? TapResult::RedrawFull : TapResult::Nothing;
      case screens::MenuTap::HowTo:
        view_ = View::HowTo;
        return TapResult::Redraw;
      case screens::MenuTap::Archive:
        if (newestDay() < 0) return TapResult::Nothing;
        showMonthOf(newestDay());
        view_ = View::Archive;
        return TapResult::Redraw;
      case screens::MenuTap::GetPuzzles:
        return wordle::lastKnownDay(answers_) >= today() && today() >= 0 ? TapResult::Nothing : beginImport();
    }
    return TapResult::Nothing;
  }

  TapResult tapGame(int x, int y) {
    if (game_.over()) return tapFinishedGame(x, y);

    const char key = screens::keyAt(x, y);
    if (key == 0) return TapResult::Nothing;

    const bool hadMessage = message_ != nullptr;
    message_ = nullptr;

    if (key == screens::KEY_ENTER) return submit(hadMessage);

    const bool changed = key == screens::KEY_ERASE ? game_.erase() : game_.type(key);
    if (changed) saveOpenGame();
    return changed || hadMessage ? TapResult::Redraw : TapResult::Nothing;
  }

  TapResult tapFinishedGame(int x, int y) {
    if (screens::isOnFinishedArchive(x, y)) {
      noteSave(storage::erase(OPEN_GAME_FILE));
      showMonthOf(game_.day());
      view_ = View::Archive;
      return TapResult::Redraw;
    }
    if (screens::isOnFinishedNext(x, y)) {
      const int next = nextUnfinished();
      return next >= 0 && openDay(next) ? TapResult::RedrawFull : TapResult::Nothing;
    }
    return TapResult::Nothing;
  }

  TapResult submit(bool hadMessage) {
    std::pair<const char*, size_t> list{reinterpret_cast<const char*>(words_.get()), wordCount_};
    const wordle::Game::Submit result = game_.submit(isListed, &list);

    switch (result) {
      case wordle::Game::Submit::Short:
      case wordle::Game::Submit::Over:
        return hadMessage ? TapResult::Redraw : TapResult::Nothing;
      case wordle::Game::Submit::NotAWord:
        message_ = "Not in the word list.";
        return TapResult::Redraw;
      case wordle::Game::Submit::Scored:
        saveGuesses();
        saveOpenGame();
        return TapResult::Redraw;
      case wordle::Game::Submit::Won:
      case wordle::Game::Submit::Lost:
        break;
    }

    saveGuesses();
    saveOpenGame();

    // The first finish is the one kept: replaying a day never rewrites it, and a finish
    // on the puzzle's own date is what the record counts.
    const uint8_t before = wordle::resultOf(results_, game_.day()) & 0x7F;
    if (before == 0 || before == wordle::kStarted) {
      const uint8_t value = result == wordle::Game::Submit::Won ? static_cast<uint8_t>(game_.guesses()) : wordle::kLost;
      const bool onTheDay = game_.day() == today();
      wordle::putResult(results_, game_.day(), static_cast<uint8_t>(value | (onTheDay ? wordle::kOnTheDay : 0)));
      saveResults();
    }
    Serial.printf("Wordle day %d %s in %d, today is day %d, newest on card is day %d\n", game_.day(), result == wordle::Game::Submit::Won ? "won" : "lost", game_.guesses(), today(), wordle::lastKnownDay(answers_));
    return TapResult::RedrawFull;
  }

  // --- The archive ---

  void showMonthOf(int day) {
    int date = 0;
    wordle::dateOfDay(day, calendarYear_, calendarMonth_, date);
    buildCalendar();
  }

  void buildCalendar() {
    for (auto& cell : calendarCells_) {
      cell = screens::CalendarDay{};
    }
    calendarPlayed_ = 0;

    const int first = wordle::dayIndex(calendarYear_, calendarMonth_, 1);
    const int next = firstOfNextMonth(calendarYear_, calendarMonth_);
    const int lead = leadingBlanks(first);
    const int newest = newestDay();

    for (int day = first; day < next; day++) {
      const int index = lead + day - first;
      if (index >= screens::CALENDAR_CELLS) break;

      screens::CalendarDay& cell = calendarCells_[index];
      cell.day = static_cast<uint8_t>(day - first + 1);
      char word[wordle::kLetters];
      cell.inArchive = day <= newest && wordle::answerFor(answers_, day, word);
      if (!cell.inArchive) continue;

      const uint8_t result = wordle::resultOf(results_, day);
      cell.played = (result & 0x7F) != 0;
      cell.finished = isFinishedResult(result);
      cell.lost = (result & 0x7F) == wordle::kLost;
      if (cell.finished) calendarPlayed_++;
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

  bool canStep(int months) const {
    int year = calendarYear_;
    int month = calendarMonth_;
    addMonths(year, month, months);
    return firstOfNextMonth(year, month) > 0 && wordle::dayIndex(year, month, 1) <= newestDay();
  }

  TapResult step(int months) {
    if (!canStep(months)) return TapResult::Nothing;
    addMonths(calendarYear_, calendarMonth_, months);
    buildCalendar();
    return TapResult::Redraw;
  }

  TapResult tapArchive(int x, int y) {
    int cell = -1;
    switch (screens::calendarTapAt(x, y, cell)) {
      case screens::CalendarTap::Nothing:
        return TapResult::Nothing;
      case screens::CalendarTap::Today:
        showMonthOf(newestDay());
        return TapResult::Redraw;
      case screens::CalendarTap::PrevYear:
        return step(-12);
      case screens::CalendarTap::NextYear:
        return step(12);
      case screens::CalendarTap::PrevMonth:
        return step(-1);
      case screens::CalendarTap::NextMonth:
        return step(1);
      case screens::CalendarTap::Day: {
        const screens::CalendarDay& day = calendarCells_[cell];
        if (day.day == 0 || !day.inArchive) return TapResult::Nothing;
        return openDay(wordle::dayIndex(calendarYear_, calendarMonth_, day.day)) ? TapResult::RedrawFull : TapResult::Nothing;
      }
    }
    return TapResult::Nothing;
  }

  void drawArchive(uint8_t* frame) const {
    screens::CalendarModel model;
    model.year = calendarYear_;
    model.month = calendarMonth_;
    model.playedThisMonth = calendarPlayed_;
    model.cells = calendarCells_;
    model.canPrevYear = canStep(-12);
    model.canNextYear = canStep(12);
    model.canPrevMonth = canStep(-1);
    model.canNextMonth = canStep(1);

    const int now = today();
    if (now >= 0) {
      int year = 0;
      int month = 0;
      int date = 0;
      wordle::dateOfDay(now, year, month, date);
      if (year == calendarYear_ && month == calendarMonth_) {
        model.todayCell = leadingBlanks(wordle::dayIndex(calendarYear_, calendarMonth_, 1)) + date - 1;
      }
    }
    screens::drawCalendar(frame, model);
  }

  // --- Getting the puzzles ---

  TapResult beginImport() {
    view_ = View::Importing;
    imported_ = 0;
    if (network::state() == network::State::Connected) {
      importStep_ = ImportStep::WaitingForTime;
      importDetail_ = "CHECKING THE DATE";
      return TapResult::Redraw;
    }
    importStep_ = ImportStep::Joining;
    importDetail_ = "JOINING WI-FI";
    return TapResult::OpenWifi;
  }

  TapResult backFromImport() {
    switch (importStep_) {
      case ImportStep::Downloading:
        wordleimport::cancel();
        importStep_ = ImportStep::Cancelling;
        importDetail_ = "STOPPING";
        return TapResult::Redraw;
      case ImportStep::Cancelling:
      case ImportStep::Saving:
        return TapResult::Nothing;
      case ImportStep::WaitingForTime:
        network::turnOff();
        return showMenu();
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
  }

  bool updateImport() {
    switch (importStep_) {
      case ImportStep::WaitingForTime:
        // Today has to be right before asking for today's answer. On a first boot the
        // time server is the only thing that knows it.
        if (network::state() != network::State::Connected) {
          finishImport(ImportStep::Failed, "NO WI-FI");
          return true;
        }
        if (network::internet() == network::Internet::Waiting) return false;
        wordleimport::begin(answers_, loadWords(), today());
        importStep_ = ImportStep::Downloading;
        importDetail_ = "DOWNLOADING";
        return true;

      case ImportStep::Downloading:
        switch (wordleimport::update()) {
          case wordleimport::Step::Working:
            return false;
          case wordleimport::Step::Failed:
            finishImport(ImportStep::Failed, wordleimport::failure());
            return true;
          case wordleimport::Step::Finished:
            importStep_ = ImportStep::Saving;
            importDetail_ = "SAVING";
            return true;
        }
        return false;

      case ImportStep::Saving:
        saveImport();
        return true;

      case ImportStep::Cancelling:
        if (!wordleimport::stopped()) return false;
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
    const std::string& words = wordleimport::words();
    if (!words.empty()) {
      if (!writeBlob(WORDS_BLOB, words)) {
        finishImport(ImportStep::Failed, "CANNOT WRITE TO CARD");
        return;
      }
      words_.reset();
    }
    if (!writeBlob(ANSWERS_BLOB, wordleimport::answers())) {
      finishImport(ImportStep::Failed, "CANNOT WRITE TO CARD");
      return;
    }
    answers_ = wordleimport::answers();
    imported_ = wordle::lastKnownDay(answers_) + 1;
    finishImport(ImportStep::Done, "TAP TO PLAY");
    Serial.printf("Wordle puzzles through day %d\n", imported_ - 1);
  }

  // --- Drawing ---

  void drawMenu(uint8_t* frame) const {
    screens::MenuModel model;
    const int day = newestDay();
    char date[20];
    char state[24];
    if (day >= 0) {
      formatDay(day, true, date, sizeof(date));
      model.date = date;
      const uint8_t result = wordle::resultOf(results_, day) & 0x7F;
      if (result >= 1 && result <= wordle::kRows) {
        snprintf(state, sizeof(state), "SOLVED IN %d", result);
        model.state = state;
      } else if (result == wordle::kLost) {
        model.state = "NOT SOLVED";
      } else if (result == wordle::kStarted) {
        model.state = "IN PROGRESS";
      }
    }
    model.stats = wordle::statsFor(results_, today());
    model.puzzles = day + 1;
    model.upToDate = day >= 0 && wordle::lastKnownDay(answers_) >= today();
    screens::drawMenu(frame, model);
  }

  void drawGame(uint8_t* frame) const {
    char date[16];
    formatDay(game_.day(), false, date, sizeof(date));
    screens::GameModel model;
    model.game = &game_;
    model.date = date;
    model.message = message_;
    model.canNext = game_.over() && nextUnfinished() >= 0;
    screens::drawGame(frame, model);
  }

  void drawImport(uint8_t* frame) const {
    screens::ImportModel model;
    model.detail = importDetail_;
    model.puzzles = imported_;
    model.done = importStep_ == ImportStep::Done;
    model.failed = importStep_ == ImportStep::Failed;
    screens::drawImport(frame, model);
  }

  View view_ = View::Menu;

  std::string answers_;  // five bytes per day, '.' for unknown
  std::string results_;  // one byte per day, in wordle_rules.h's encoding
  storage::Blob words_;
  size_t wordCount_ = 0;

  wordle::Game game_;
  const char* message_ = nullptr;

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

Game& wordleGame() {
  static WordleGame game;
  return game;
}
