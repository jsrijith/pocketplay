#pragma once

#include "pocketbgg_list.h"

// Shown until the first sync, which replaces it (M5a), until saving arrives (M5b) and this goes. The first five are real games from a BGG
// collection, chosen because each tests something: the longest name, an en dash and a quote
// that the parser will have to turn into ASCII, a year BC, an ampersand, and no year at all.
// The rest are made up, to fill a few pages.
namespace pocketbgg {

struct SampleGame {
  uint32_t id;
  const char* name;
  int16_t year;
  uint16_t plays;
};

constexpr SampleGame kSampleGames[] = {
    {310953, "Pokemon Trading Card Game: Battle Academy", 2020, 33},
    {301919, "Pandemic: Hot Zone - North America", 2020, 5},
    {3886, "Nine Men's Morris", -1400, 3},
    {270673, "Silver & Gold", 2019, 6},
    {102750, "Math Dice Jr.", kNoYear, 2},
    {1, "Sample Game: The Long Way Round to Somewhere", 2021, 0},
    {2, "Sample Game Two", 2018, 1},
    {3, "Sample Three", 2015, 12},
    {4, "Sample Four", 2012, 206},
    {5, "Sample Five", 1999, 40},
    {6, "Sample Six", 2024, 7},
    {7, "Sample Seven: Expansion Pack", 2023, 2},
    {8, "Sample Eight", 2010, 18},
    {9, "Sample Nine", 2005, 9},
    {10, "Sample Ten", 2016, 64},
    {11, "Sample Eleven", 2020, 3},
    {12, "Sample Twelve", 2008, 0},
    {13, "Sample Thirteen", 2019, 27},
    {14, "Sample Fourteen", 2022, 5},
    {15, "Sample Fifteen", 2014, 11},
};
inline std::vector<Game> sampleGames() {
  std::vector<Game> games;
  for (const SampleGame& s : kSampleGames) {
    Game g;
    g.id = s.id;
    g.name = s.name;
    g.year = s.year;
    g.plays = s.plays;
    g.status = kOwn;
    games.push_back(g);
  }
  return games;
}

}  // namespace pocketbgg
