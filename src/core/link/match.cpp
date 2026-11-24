#include "match.h"

#include "radio.h"

namespace linkplay {
namespace {

// The only two things one device tells the other outside a move. CrossPlay's values,
// so the two firmwares understand each other.
constexpr uint8_t kNotePlayAgain = 1;
constexpr uint8_t kNoteLeaving = 2;

// The match in progress, for the goodbye before sleep. One at a time.
Match* activeMatch = nullptr;

void goodbyeBeforeSleep() {
  if (activeMatch != nullptr) activeMatch->leave();
}

}  // namespace

// Quietly: the game holding this may be part way through being destroyed, so it
// isn't called back.
Match::~Match() {
  if (!requested_) return;
  play_.stop();
  if (activeMatch == this) {
    activeMatch = nullptr;
    setBeforeSleep(nullptr);
  }
}

bool Match::enter(const GameId gameId, const char* yourName) {
  leave();
  if (!play_.start(gameId, yourName)) return false;
  requested_ = true;
  rematch_ = false;
  endgame_.reset();
  lastPhase_ = play_.phase();
  you_ = SeatState::Ready;
  them_ = SeatState::Looking;
  changed_ = true;
  activeMatch = this;
  setBeforeSleep(&goodbyeBeforeSleep);
  return true;
}

void Match::leave() {
  if (!requested_) return;
  // Only while ANOTHER GAME? is open. stop() sends a goodbye anyway, but this makes
  // their seat say LEFT at once.
  if (rematch_) play_.say(kNoteLeaving);
  play_.stop();
  requested_ = false;
  rematch_ = false;
  endgame_.reset();
  lastPhase_ = PlayBase::Phase::Off;
  if (activeMatch == this) {
    activeMatch = nullptr;
    setBeforeSleep(nullptr);
  }
  host_.onLinkEnded();
}

bool Match::inMatch() const {
  return requested_ && play_.phase() != PlayBase::Phase::Off && play_.phase() != PlayBase::Phase::Searching;
}

bool Match::keepAwake() const {
  if (!requested_) return false;
  const PlayBase::Phase phase = play_.phase();
  if (phase == PlayBase::Phase::Searching) return true;
  const bool playing = phase == PlayBase::Phase::YourTurn || phase == PlayBase::Phase::TheirTurn;
  return playing && !rematch_ && !host_.matchGameOver();
}

bool Match::ownsScreen() const {
  if (!requested_) return false;
  // The final board comes first, even if they leave in the same second: the player
  // who lost should see the move that beat them.
  if (endgame_.showingFinal()) return false;
  return play_.phase() == PlayBase::Phase::Searching || play_.phase() == PlayBase::Phase::Over || rematch_;
}

const char* Match::interruption() const {
  if (host_.matchGameOver()) return nullptr;
  // Not "they left" for a silence: the battery chose, not them.
  if (them_ == SeatState::Lost) return "LOST CONNECTION";
  if (them_ == SeatState::Left) return "THEY LEFT";
  return nullptr;
}

bool Match::update(const uint32_t nowMs) {
  if (!requested_) return false;
  play_.update(nowMs);

  if (play_.takeMatchStart()) {
    rematch_ = false;
    you_ = SeatState::Deciding;
    them_ = SeatState::Deciding;
    endgame_.reset();
    host_.onMatchStart(play_.goesFirst());
    changed_ = true;
  }

  // Notes before moves: an answer can end the wait in this same pass.
  uint8_t note = 0;
  if (play_.heard(note)) {
    if (note == kNoteLeaving) {
      them_ = SeatState::Left;
      rematch_ = true;
      changed_ = true;
    } else if (note == kNotePlayAgain) {
      them_ = SeatState::Ready;
      rematch_ = true;
      if (you_ == SeatState::Ready) {
        startRematch();
      } else {
        you_ = SeatState::Deciding;  // they asked; we haven't answered
        changed_ = true;
      }
    }
  }

  if (host_.takeOpponentState()) changed_ = true;

  // The final board is held, and the match counted, before ANOTHER GAME? is asked.
  struct EndgameHost {
    Match& self;
    bool matchGameOver() const { return self.host_.matchGameOver(); }
    void onMatchEnded() const { self.host_.onMatchEnded(); }
    void onEndgameChanged() const { self.changed_ = true; }
  } endgameHost{*this};
  endgame_.update(endgameHost, inMatch(), nowMs);
  if (endgame_.offering() && !rematch_) {
    rematch_ = true;
    you_ = SeatState::Deciding;
    them_ = SeatState::Deciding;
    changed_ = true;
  }

  if (play_.phase() == PlayBase::Phase::Over) {
    // Gone for good, so there's nothing more to ask them. The radio stays on until
    // leave(), and keepAwake() lets the device sleep meanwhile.
    them_ = play_.ending() == PlayBase::Ending::OpponentLost ? SeatState::Lost : SeatState::Left;
    rematch_ = true;
  }

  if (play_.phase() != lastPhase_) {
    lastPhase_ = play_.phase();
    changed_ = true;
  }

  const bool changed = changed_;
  changed_ = false;
  return changed;
}

void Match::proposeRematch() {
  // Only an answer they actually gave counts, not a seat left on Ready from pairing.
  const bool theyAlreadyAgreed = rematch_ && them_ == SeatState::Ready;
  endgame_.skip();
  you_ = SeatState::Ready;
  play_.say(kNotePlayAgain);
  if (theyAlreadyAgreed) {
    startRematch();
    return;
  }
  rematch_ = true;
  if (them_ != SeatState::Left && them_ != SeatState::Lost) them_ = SeatState::Deciding;
  changed_ = true;
}

void Match::startRematch() {
  endgame_.reset();
  rematch_ = false;
  you_ = SeatState::Deciding;
  them_ = SeatState::Deciding;
  host_.onMatchStart(play_.goesFirst());
  changed_ = true;
}

}  // namespace linkplay
