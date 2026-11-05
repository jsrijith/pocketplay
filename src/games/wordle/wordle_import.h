#pragma once

#include <string>

// Getting the puzzles: every past answer and the accepted guesses from a mirror, then
// the days the mirror hasn't caught up with from NYT. It runs one download at a time
// and keeps the results in memory. The game writes them to the card once it's done.
namespace wordleimport {

enum class Step { Working, Finished, Failed };

// answers is what the card holds now, and today is -1 with no clock. The guess list
// and the archive are fetched only if they're missing or too far behind.
void begin(const std::string& answers, bool haveWords, int today);

// Call every loop. Starts each download as the last one ends.
Step update();

void cancel();

// True once no download is running, so the radio can be turned off.
bool stopped();

const char* failure();
const std::string& answers();
const std::string& words();  // empty if the card's list is still good

}  // namespace wordleimport
