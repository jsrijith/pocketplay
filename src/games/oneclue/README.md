# One Clue

A deck of word cards for a cooperative party game played round a table. PocketPlay only deals the cards. Clues, guesses and the score stay on paper.

**How it plays:** the screen shows one card of five numbered words. The guesser picks a number without looking. Everyone else writes a one-word clue for that word, and clues that match are cancelled. The guesser then gets one guess from the clues that are left.

- `NEXT CARD`, or the Down button, turns over a card that hasn't been shown since the last shuffle. The band shows how far through the deck you are, such as `12/110`.
- After all 110 cards, `NEXT CARD` dims, and a line says `NEW GAME` reshuffles.
- `NEW GAME` shuffles all 110 cards and shows the first one, with a full refresh.
- `BACK` returns to the home page and keeps the deck.

**Saves:** `oneclue`, version 1, 111 bytes: the shuffled order of the 110 cards, then how many have been shown. It's written on every `NEXT CARD` and `NEW GAME`, so after a sleep, a crash or a flat battery you get the same card back, and used cards don't come back. The deck has no end, so this save is never erased. If a save is missing or doesn't load, the deck is shuffled fresh.

**The words:** `oneclue_cards.h` is the owner's list of 110 cards, in its original order, in capitals. These changes were made:

- Spellings fixed: Cemetary to Cemetery, Cupic to Cupid, Antartica to Antarctica, Sombero to Sombrero, Austrailia to Australia, Toliken to Tolkien, Magned to Magnet, Starbuks to Starbucks, Roison to Poison, Neghborhood to Neighborhood, Colcano to Volcano, Frankestein to Frankenstein, Pacasso to Picasso, Flitstone to Flintstone, Launguage to Language.
- Cards 27 and 37 had four words. **Kangaroo** was added to 27 and **Saturn** to 37.
- Three words are on two cards each, as in the list: Glasses (37 and 55), Crossroads (61 and 95) and Rocky (66 and 80).

Every word is drawn at Size30, because one cut is shared by the whole deck and Size44 doesn't fit most of the longer words.

**Code:** written for PocketPlay; nothing is copied from CrossPlay. `oneclue_deck.h` shuffles the deck with Fisher-Yates and xorshift32, seeded from the chip's hardware random numbers. `oneclue_save.cpp` packs the deck. Both are tested in `tests/oneclue/`.

**Icon:** `oneclue_icon.h`, a card with five numbered lines, drawn for PocketPlay.
