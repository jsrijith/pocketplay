#pragma once

// The half of a two-device match that's the same in every game: starting to look,
// noticing the match begin, holding the final board, asking for another game, and
// noticing the other player go. Taken from CrossPlay's LinkActivity
// (src/apps_local/link/LinkActivity.cpp, MIT), without its screens or input, so it
// can be tested on the PC. ui/link/ draws it.
//
// A game owns a Play<State> and then a Match, declared in that order so the Play
// outlives the Match, implements MatchHost, and then:
//
//   PLAY NEARBY      match.enter(GameId::Yahtzee)  (false: the radio wouldn't start)
//   update()         return match.update(millis()) || anything of its own
//   onTap()          if (match.ownsScreen()) route the tap to ui/link instead
//                    on your turn, after a move: play.play(state)
//   draw()           if (match.ownsScreen()) draw ui/link's screen, else the board,
//                    then match.notePainted(millis())
//   busy()           match.keepAwake()
//   BACK             match.leave()
//
// Sleep tells the other device too: enter() hands the radio a goodbye, which
// shell::beforeSleep() calls.

#include <cstdint>

#include "endgame.h"
#include "play.h"

namespace linkplay {

// What each seat on the match screen says.
enum class SeatState : uint8_t {
  Looking,   // nobody there yet
  Deciding,  // asked, and hasn't answered
  Ready,     // said yes to another game
  Left,      // chose to stop, or closed the game
  Lost,      // went quiet: flat battery, out of range
};

// What only the game knows.
class MatchHost {
 public:
  // A match has begun, or both said yes to another. Set up a fresh game. `goesFirst`
  // comes from a coin toss both devices agree on.
  virtual void onMatchStart(bool goesFirst) = 0;
  // Adopt whatever the opponent sent, usually play.takeOpponent(state). True if the
  // screen changed.
  virtual bool takeOpponentState() = 0;
  virtual bool matchGameOver() const = 0;
  // The game has just finished, once, on both devices. Record the result and show the
  // final board; Match holds it on screen for a moment before asking for another.
  virtual void onMatchEnded() = 0;
  // The match is over and the game is solo again.
  virtual void onLinkEnded() = 0;

 protected:
  ~MatchHost() = default;
};

class Match {
 public:
  Match(PlayBase& play, MatchHost& host) : play_(play), host_(host) {}
  ~Match();
  Match(const Match&) = delete;
  Match& operator=(const Match&) = delete;

  // Starts the radio and looks for a player. False, with nothing changed, if the
  // radio won't start.
  bool enter(GameId gameId, const char* yourName);
  // Tells the other device, turns the radio off, and calls onLinkEnded(). Safe in any
  // state.
  void leave();

  // Once per loop. True when something on screen changed.
  bool update(uint32_t nowMs);

  // ANOTHER GAME? answered yes, or asked for. Starts it if they've already said yes.
  void proposeRematch();

  // The game asked for this, until leave().
  bool active() const { return requested_; }
  // Looking for a player, or a game going on: waiting for the other player to move
  // looks idle, and a sleep would end the match. Once a game is over, or they've
  // gone, the idle timer runs again, and sleeping still says goodbye. (CrossPlay keeps
  // the device awake until the player leaves, which can be forever on THEY LEFT.)
  bool keepAwake() const;
  // Paired with someone, rather than still looking.
  bool inMatch() const;
  bool yourTurn() const { return requested_ && play_.phase() == PlayBase::Phase::YourTurn; }
  // The match screen is up instead of the game's: looking, the other player gone, or
  // ANOTHER GAME?. Never while the final board is being held.
  bool ownsScreen() const;
  // Call after drawing the game's own screen, so the final board is held from when it
  // was shown rather than from the move.
  void notePainted(uint32_t nowMs) { endgame_.notePainted(endgame_.stage(), nowMs); }

  // For the match screen.
  SeatState you() const { return you_; }
  SeatState them() const { return them_; }
  const char* opponentName() const { return play_.opponentName(); }
  bool linked() const { return requested_ && play_.phase() != PlayBase::Phase::Searching; }
  bool offerPlayAgain() const { return rematch_ && them_ != SeatState::Left && them_ != SeatState::Lost; }
  // "THEY LEFT" or "LOST CONNECTION" when that's why the screen is up in the middle of
  // a game, or nullptr, when the game's own headline applies.
  const char* interruption() const;

 private:
  void startRematch();

  PlayBase& play_;
  MatchHost& host_;
  bool requested_ = false;
  // ANOTHER GAME? is being asked, by either side or by the game ending.
  bool rematch_ = false;
  bool changed_ = false;
  PlayBase::Phase lastPhase_ = PlayBase::Phase::Off;
  Endgame endgame_;
  SeatState you_ = SeatState::Ready;
  SeatState them_ = SeatState::Looking;
};

}  // namespace linkplay
