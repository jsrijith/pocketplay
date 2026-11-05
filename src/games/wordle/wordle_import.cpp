#include "wordle_import.h"

#include <Arduino.h>

#include <cstdio>
#include <cstring>

#include "core/download.h"
#include "wordle_rules.h"

namespace wordleimport {

namespace {

constexpr const char* ERA1_URL = "https://raw.githubusercontent.com/mfilej/wrdl/main/solutions-era1.txt";
constexpr const char* RECENT_URL = "https://raw.githubusercontent.com/mfilej/wrdl/main/solutions.txt";
constexpr const char* WORDS_URL = "https://raw.githubusercontent.com/mfilej/wrdl/main/valid.txt";
constexpr const char* DAILY_URL = "https://www.nytimes.com/svc/wordle/v2/%04d-%02d-%02d.json";

// A gap longer than this is fetched from the archive rather than day by day.
constexpr int MAX_DAILY_FETCHES = 14;
constexpr size_t MIN_WORD_LIST_BYTES = 1000 * wordle::kLetters;
constexpr size_t EXPECTED_WORD_LIST_BYTES = 80000;
constexpr size_t MAX_LINE_BYTES = 64;
constexpr size_t MAX_DAILY_BYTES = 4096;

enum class Stage { Era1, Recent, Words, Daily, Finished, Failed };

Stage stage = Stage::Failed;
bool waiting = false;  // a download for this stage has been started
bool cancelled = false;
const char* failureText = "";

std::string answerData;
std::string wordData;
std::string dailyBody;
std::string partialLine;
bool wordsSorted = true;
char dailyUrl[80];
int currentDay = 0;
int dailyFetches = 0;
int todayIndex = -1;

// Download task only, until it ends: these touch memory and nothing else.
void addAnswerLine(const char* line, size_t size) {
  int day = -1;
  char word[wordle::kLetters];
  if (wordle::parseAnswerLine(line, size, day, word)) wordle::putAnswer(answerData, day, word);
}

void addWordLine(const char* line, size_t size) {
  while (size > 0 && (line[size - 1] == '\r' || line[size - 1] == ' ')) size--;
  if (size != wordle::kLetters) return;

  char word[wordle::kLetters];
  for (int i = 0; i < wordle::kLetters; i++) {
    char c = line[i];
    if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    if (c < 'A' || c > 'Z') return;
    word[i] = c;
  }
  if (!wordData.empty() && memcmp(wordData.data() + wordData.size() - wordle::kLetters, word, wordle::kLetters) >= 0) wordsSorted = false;
  wordData.append(word, wordle::kLetters);
}

// Lines split across network chunks are carried over to the next chunk.
bool feedLines(const uint8_t* data, size_t size, void (*onLine)(const char*, size_t)) {
  for (size_t i = 0; i < size; i++) {
    const char c = static_cast<char>(data[i]);
    if (c == '\n') {
      onLine(partialLine.data(), partialLine.size());
      partialLine.clear();
    } else if (partialLine.size() < MAX_LINE_BYTES) {
      partialLine += c;
    }
  }
  return true;
}

bool writeAnswers(void*, const uint8_t* data, size_t size) { return feedLines(data, size, addAnswerLine); }

bool writeWords(void*, const uint8_t* data, size_t size) { return feedLines(data, size, addWordLine); }

bool writeDaily(void*, const uint8_t* data, size_t size) {
  if (dailyBody.size() + size > MAX_DAILY_BYTES) return false;
  dailyBody.append(reinterpret_cast<const char*>(data), size);
  return true;
}

// Answers are written by date, so reading the same lines twice does no harm.
void restartAnswers(void*) { partialLine.clear(); }

void restartWords(void*) {
  partialLine.clear();
  wordData.clear();
  wordsSorted = true;
}

void restartDaily(void*) { dailyBody.clear(); }

void fail(const char* text) {
  failureText = text;
  stage = Stage::Failed;
  Serial.printf("Wordle puzzles: %s, HTTP status %d\n", text, download::httpStatus());
}

bool startFetch(const char* url, const download::Sink& sink) {
  partialLine.clear();
  if (!download::start(url, sink)) {
    fail("DOWNLOAD FAILED");
    return false;
  }
  waiting = true;
  return true;
}

// The last line of a file may have no line ending.
void finishLines(void (*onLine)(const char*, size_t)) {
  if (!partialLine.empty()) onLine(partialLine.data(), partialLine.size());
  partialLine.clear();
}

void startNextDaily() {
  // Never past today, so nothing on the card is a word nobody has played yet.
  currentDay = wordle::lastKnownDay(answerData) + 1;
  if (currentDay > todayIndex || dailyFetches >= MAX_DAILY_FETCHES) {
    stage = Stage::Finished;
    return;
  }
  int year = 0;
  int month = 0;
  int day = 0;
  wordle::dateOfDay(currentDay, year, month, day);
  snprintf(dailyUrl, sizeof(dailyUrl), DAILY_URL, year, month, day);
  dailyBody.clear();
  dailyFetches++;
  startFetch(dailyUrl, download::Sink{nullptr, writeDaily, restartDaily});
}

}  // namespace

void begin(const std::string& answers, bool haveWords, int today) {
  answerData = answers;
  wordData.clear();
  dailyBody.clear();
  partialLine.clear();
  wordsSorted = true;
  waiting = false;
  cancelled = false;
  failureText = "";
  dailyFetches = 0;
  todayIndex = today;

  const int held = wordle::lastKnownDay(answerData);
  stage = !haveWords || held < 0 || today - held > MAX_DAILY_FETCHES ? Stage::Era1 : Stage::Daily;
}

Step update() {
  if (waiting) {
    if (download::state() == download::State::Running) return Step::Working;
    waiting = false;
    const bool succeeded = download::state() == download::State::Done;

    if (cancelled) {
      fail("CANCELLED");
    } else if (stage == Stage::Daily) {
      // A missing day isn't a failure: the puzzles already held are still good.
      int day = -1;
      char word[wordle::kLetters];
      if (succeeded && wordle::parseDailyJson(dailyBody, day, word) && day == currentDay) {
        wordle::putAnswer(answerData, day, word);
      } else {
        Serial.printf("Wordle day %d from NYT: HTTP status %d\n", currentDay, download::httpStatus());
        stage = Stage::Finished;
      }
    } else if (!succeeded) {
      fail("DOWNLOAD FAILED");
    } else if (stage == Stage::Era1) {
      finishLines(addAnswerLine);
      stage = Stage::Recent;
    } else if (stage == Stage::Recent) {
      finishLines(addAnswerLine);
      stage = Stage::Words;
    } else if (stage == Stage::Words) {
      finishLines(addWordLine);
      if (wordle::lastKnownDay(answerData) < 0 || wordData.size() < MIN_WORD_LIST_BYTES) {
        fail("DOWNLOAD FAILED");
      } else if (!wordsSorted) {
        fail("BAD DATA");
      } else {
        stage = Stage::Daily;
      }
    }
  }

  if (cancelled && stage != Stage::Failed) fail("CANCELLED");

  switch (stage) {
    case Stage::Era1:
      startFetch(ERA1_URL, download::Sink{nullptr, writeAnswers, restartAnswers});
      break;
    case Stage::Recent:
      startFetch(RECENT_URL, download::Sink{nullptr, writeAnswers, restartAnswers});
      break;
    case Stage::Words:
      wordData.reserve(EXPECTED_WORD_LIST_BYTES);
      startFetch(WORDS_URL, download::Sink{nullptr, writeWords, restartWords});
      break;
    case Stage::Daily:
      startNextDaily();
      break;
    case Stage::Finished:
    case Stage::Failed:
      break;
  }

  if (stage == Stage::Finished) return Step::Finished;
  if (stage == Stage::Failed) return Step::Failed;
  return Step::Working;
}

void cancel() {
  cancelled = true;
  download::requestCancel();
}

bool stopped() { return download::state() != download::State::Running; }

const char* failure() { return failureText; }

const std::string& answers() { return answerData; }

const std::string& words() { return wordData; }

}  // namespace wordleimport
