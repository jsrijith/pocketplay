// Klondike solitaire, ported from CrossPlay's SolitaireActivity.cpp. The only game
// drawn in landscape, because seven columns of cards want a wide screen.
#include "solitaire.h"

#include <Arduino.h>
#include <esp_random.h>

#include <vector>

#include "core/storage.h"
#include "solitaire_icon.h"
#include "solitaire_record.h"
#include "solitaire_rules.h"
#include "solitaire_save.h"
#include "solitaire_screens.h"
#include "ui/ui.h"

namespace {

namespace sui = solitaireui;

constexpr const char* BOARD_FILE = "solitaire";
constexpr const char* RESULTS_BLOB = "solitaire-results";

class Solitaire final : public Game {
 public:
  const char* name() const override { return "SOLITAIRE"; }
  const uint8_t* icon() const override { return solitaireIcon::ICON; }
  bool landscape() const override { return true; }

  // The front door every time, as in CrossPlay, with the board in play offered as
  // PICK IT UP.
  void open() override {
    if (!loaded_) {
      loadGame();
      loadResults();
      loaded_ = true;
    }
    view_ = View::Menu;
    selectedPile_ = -1;
  }

  TapResult onTap(int x, int y) override {
    if (ui::isOnBack(x, y)) return back();
    switch (view_) {
      case View::Menu:
        return tapMenu(x, y);
      case View::Board:
        return tapBoard(x, y);
      case View::Won:
        return tapWon(x, y);
    }
    return TapResult::Nothing;
  }

  void draw(uint8_t* frame) override {
    switch (view_) {
      case View::Menu: {
        sui::MenuModel model;
        model.hasSave = hasGame_ && !game_.won();
        model.savedMoves = game_.moveCount();
        model.drawThree = drawThree_;
        model.record = solitaire::summarise(results_);
        sui::drawMenu(frame, model);
        break;
      }
      case View::Board:
        sui::drawBoard(frame, boardModel());
        break;
      case View::Won: {
        const solitaire::Record record = solitaire::summarise(results_);
        sui::WinModel model;
        model.moves = game_.moveCount();
        model.wins = record.wins;
        model.streak = record.streak;
        sui::drawWin(frame, model);
        break;
      }
    }
  }

  bool saveFailed() const override { return boardFailed_ || resultsFailed_; }

  void retrySave() override {
    if (boardFailed_) persist();
    if (resultsFailed_) saveResults();
  }

 private:
  enum class View : uint8_t { Menu, Board, Won };

  sui::BoardModel boardModel() const {
    sui::BoardModel model;
    model.game = &game_;
    model.selectedPile = selectedPile_;
    model.selectedCard = selectedCard_;
    return model;
  }

  TapResult back() {
    switch (view_) {
      case View::Menu:
        return TapResult::Leave;
      case View::Won:
        view_ = View::Menu;
        return TapResult::Redraw;
      case View::Board:
        selectedPile_ = -1;
        view_ = View::Menu;
        return TapResult::Redraw;
    }
    return TapResult::Nothing;
  }

  TapResult tapMenu(int x, int y) {
    sui::MenuModel model;
    model.hasSave = hasGame_ && !game_.won();
    switch (sui::menuTapAt(model, x, y)) {
      case sui::MenuTap::Nothing:
        return TapResult::Nothing;
      case sui::MenuTap::Resume:
        selectedPile_ = -1;
        view_ = View::Board;
        return TapResult::RedrawFull;
      case sui::MenuTap::Deal:
        abandonIfPlayed();
        return newDeal();
      case sui::MenuTap::DrawMode:
        // Takes effect on the next deal, so the game in play is safe.
        drawThree_ = !drawThree_;
        return TapResult::Redraw;
    }
    return TapResult::Nothing;
  }

  TapResult tapWon(int x, int y) {
    switch (sui::winTapAt(x, y)) {
      case sui::WinTap::Nothing:
        return TapResult::Nothing;
      case sui::WinTap::Again:
        return newDeal();
      case sui::WinTap::Menu:
        view_ = View::Menu;
        return TapResult::Redraw;
    }
    return TapResult::Nothing;
  }

  TapResult tapBoard(int x, int y) {
    const sui::BoardModel model = boardModel();
    sui::layoutBoard(model, layout_);
    const sui::HitArea* area = layout_.hitAt(x, y);
    if (area == nullptr) return TapResult::Nothing;

    switch (area->hit) {
      case sui::BoardHit::Nothing:
        return TapResult::Nothing;
      case sui::BoardHit::Undo:
        if (!game_.canUndo()) return TapResult::Nothing;
        selectedPile_ = -1;
        game_.undo();
        persist();
        return TapResult::Redraw;
      case sui::BoardHit::New:
        // Leaving a deal after a move counts as a loss, so restarting every deal
        // that went badly can't farm a streak.
        abandonIfPlayed();
        return newDeal();
      case sui::BoardHit::Finish:
        selectedPile_ = -1;
        game_.autoPlay();
        return afterMove();
      case sui::BoardHit::Pile:
        return tapPile(area->pile, y);
    }
    return TapResult::Nothing;
  }

  TapResult tapPile(int pile, int y) {
    const int card = layout_.cardAt(game_, pile, y);

    // The stock is a button that looks like a pile.
    if (pile == solitaire::kStockPile) {
      const bool wasHolding = selectedPile_ >= 0;
      selectedPile_ = -1;
      if (!game_.drawFromStock()) return wasHolding ? TapResult::Redraw : TapResult::Nothing;
      return afterMove();
    }

    if (selectedPile_ < 0) {
      if (card < 0) return TapResult::Nothing;
      // A card that can go straight to a foundation goes there, because that's
      // nearly always the move meant.
      const int run = game_.runLength(pile, card);
      if (run == 1) {
        const int foundation = game_.foundationFor(pile);
        if (foundation >= 0) {
          game_.move(pile, foundation, 1);
          return afterMove();
        }
      }
      if (run == 0) return TapResult::Nothing;
      selectedPile_ = pile;
      selectedCard_ = card;
      return TapResult::Redraw;
    }

    // A second tap on the pile in hand puts it down.
    if (pile == selectedPile_) {
      selectedPile_ = -1;
      return TapResult::Redraw;
    }

    const int count = game_.pile(selectedPile_).count - selectedCard_;
    if (game_.move(selectedPile_, pile, count)) {
      selectedPile_ = -1;
      return afterMove();
    }

    // Not a legal place: pick up what was tapped instead, so a mistap costs one tap.
    // An empty column keeps the card in hand, because tapping it means "put it here".
    const int run = game_.runLength(pile, card);
    if (run > 0) {
      selectedPile_ = pile;
      selectedCard_ = card;
      return TapResult::Redraw;
    }
    if (!game_.pile(pile).empty()) {
      selectedPile_ = -1;
      return TapResult::Redraw;
    }
    return TapResult::Nothing;
  }

  // Saves, then settles a win if that move made one: the result is recorded once,
  // the save goes, and the win gets the full refresh.
  TapResult afterMove() {
    if (!game_.won()) {
      persist();
      return TapResult::Redraw;
    }
    hasGame_ = false;
    selectedPile_ = -1;
    persist();
    recordResult(true);
    view_ = View::Won;
    return TapResult::RedrawFull;
  }

  void abandonIfPlayed() {
    if (hasGame_ && !game_.won() && game_.moveCount() > 0) recordResult(false);
  }

  TapResult newDeal() {
    // The chip's hardware random numbers, so two deals after a wake differ.
    game_.deal(esp_random(), drawThree_);
    hasGame_ = true;
    selectedPile_ = -1;
    view_ = View::Board;
    persist();
    return TapResult::RedrawFull;
  }

  // --- Saving ---

  void persist() {
    if (!storage::ready()) return;
    if (!hasGame_ || game_.won()) {
      boardFailed_ = !storage::erase(BOARD_FILE);
      return;
    }
    uint8_t data[solitaire::MAX_SAVE_BYTES];
    const size_t size = solitaire::packGame(game_, data);
    const uint32_t startUs = micros();
    boardFailed_ = !storage::save(BOARD_FILE, solitaire::SAVE_VERSION, data, size);
    Serial.printf("Saved %s (%u bytes) in %lu us\n", BOARD_FILE, static_cast<unsigned>(size), static_cast<unsigned long>(micros() - startUs));
  }

  void loadGame() {
    if (!storage::ready()) return;
    uint8_t data[solitaire::MAX_SAVE_BYTES];
    uint8_t version = 0;
    size_t size = 0;
    if (!storage::load(BOARD_FILE, version, data, sizeof(data), size)) return;
    if (version != solitaire::SAVE_VERSION || !solitaire::unpackGame(data, size, game_)) {
      Serial.println("Solitaire: the save didn't check out, so it's set aside");
      return;
    }
    hasGame_ = !game_.won();
    if (hasGame_) drawThree_ = game_.drawThree();
  }

  void loadResults() {
    if (!storage::ready()) return;
    size_t size = 0;
    const storage::Blob blob = storage::loadBlob(RESULTS_BLOB, size);
    if (!blob) return;
    if (!solitaire::resultsValid(blob.get(), size)) {
      Serial.println("Solitaire: the record didn't check out, so it starts again");
      return;
    }
    results_.assign(blob.get(), blob.get() + size);
  }

  void recordResult(bool won) {
    solitaire::appendResult(results_, won);
    saveResults();
  }

  void saveResults() {
    if (!storage::ready()) return;
    resultsFailed_ = !storage::saveBlob(RESULTS_BLOB, results_.data(), results_.size());
  }

  solitaire::Game game_;
  sui::Layout layout_;
  std::vector<uint8_t> results_;
  View view_ = View::Menu;
  bool loaded_ = false;
  bool hasGame_ = false;
  bool drawThree_ = false;
  int selectedPile_ = -1;
  int selectedCard_ = 0;
  bool boardFailed_ = false;
  bool resultsFailed_ = false;
};

Solitaire instance;

}  // namespace

Game& solitaireGame() { return instance; }
