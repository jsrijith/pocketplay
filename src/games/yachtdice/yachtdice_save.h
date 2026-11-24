#pragma once

#include <cstddef>
#include <cstdint>

#include "yachtdice_rules.h"

// Yacht Dice's saves as bytes. No screen or card code, so tests/yachtdice/ can
// check them. The rules are CrossPlay's and keep its namespace, yahtzee.
namespace yachtdice {

// The game in play, in the save named "yachtdice", erased once it ends. You always
// play seat 0 against the built-in opponent.
//
//   [0..12]   your card, one byte a box, 0xFF for a box not yet filled
//   [13]      your extra five-of-a-kind bonuses
//   [14..26]  the opponent's card
//   [27]      the opponent's bonuses
//   [28..32]  the five dice, 1-6
//   [33]      which dice are held, a bit each
//   [34]      rolls used this turn, 0-3
//   [35]      whose turn: 0 yours, 1 theirs
//   [36..39]  the random state the next rolls come from, low byte first
constexpr uint8_t GAME_VERSION = 1;
constexpr size_t GAME_BYTES = 40;

void packGame(const yahtzee::Game& game, uint8_t* out);

// False for a wrong size, or anything yahtzee::plausible() refuses (a score no box
// can hold, a die past 6, cards out of step with the turn), or a finished game,
// which is never saved.
bool unpackGame(const uint8_t* data, size_t size, yahtzee::Game& game);

// The record, in the save named "yachtdice-record", as CrossPlay keeps it: games
// played and won, the best total, how many times five of a kind was rolled, and the
// face of the last one, which the menu draws.
//
//   [0..1]    played
//   [2..3]    won
//   [4..5]    best total
//   [6..7]    five of a kinds rolled
//   [8]       the last one's face, 1-6, or 0 for none yet
constexpr uint8_t RECORD_VERSION = 1;
constexpr size_t RECORD_BYTES = 9;

// The highest total a card can reach: every box at its best, the top bonus, and
// twelve extra five of a kinds.
constexpr int MAX_TOTAL = 1575;

struct Record {
  int played = 0;
  int won = 0;
  int best = 0;
  int fives = 0;
  int lastFace = 0;
};

void packRecord(const Record& record, uint8_t* out);

// False for a wrong size, more won than played, a best past MAX_TOTAL or with
// nothing played, or a face past 6.
bool unpackRecord(const uint8_t* data, size_t size, Record& record);

}  // namespace yachtdice
