#pragma once

#include <cstdint>

#include "core/link/match.h"

// The match screen every two-device game shares, drawn while core/link/match.h says
// it owns the screen: LOOKING FOR A PLAYER, ANOTHER GAME?, or the other player gone.
// Two seats, YOU and them, each saying what that player is doing, and the buttons
// under them. Laid out after CrossPlay's link screen, in PocketPlay's design.
namespace linkui {

// `gameHeadline` is the game's own sentence for a finished game, such as "YOU WIN".
// Searching and the other player leaving are said here, the same in every game.
void draw(uint8_t* frame, const linkplay::Match& match, const char* gameTitle, const char* gameHeadline);

enum class Tap {
  Nothing,
  PlayAgain,  // call match.proposeRematch()
  Leave,      // BACK, CANCEL, LEAVE or DONE: call match.leave()
};

Tap tapAt(const linkplay::Match& match, int x, int y);

}  // namespace linkui
