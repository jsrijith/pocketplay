# PocketBGG

Your BoardGameGeek collection, to browse on the device.

**Work in progress.** It can't sync yet, so it shows sample games, marked `SAMPLE DATA` at the top. Five of them are real BGG games, chosen to test long names and unusual years; the rest are made up.

## Setting up

PocketBGG needs your BGG username and an API token, which BGG gives to each person who registers to use its data.

1. On [boardgamegeek.com/applications](https://boardgamegeek.com/applications), register a non-commercial application if you haven't already. BGG has to approve it, which can take a few days.
2. Open the approved application and use **Create token**. Use the API token, not the MCP one.
3. Open PocketBGG once, so it makes the folder `/pocketbgg` on the SD card.
4. Put the card in a computer, and in that folder make a text file named `config.txt`, with Notepad for example:
   ```
   username=your_bgg_username
   token=your-token
   ```
5. Put the card back and tap `SYNC`.

The token is read from the card each time you sync, so a corrected file counts straight away. It's never shown, logged or saved anywhere else, and it's sent only to boardgamegeek.com, after the device has checked BGG's certificate. Anyone holding your card can read the file, though, so if you lose the card, delete the token on BGG's applications page and make a new one.

If `SYNC` doesn't get as far as Wi-Fi, the line above it says what's wrong with `config.txt`, such as `CONFIG.TXT HAS NO TOKEN=`.

## Getting around

- The list shows each game's name, with the year and how many times it's been played under it. Long names wrap onto a second line rather than being cut short.
- The Up and Down buttons turn the pages. `PAGE 1 OF 4` at the top says where you are.
- `SYNC` will download your collection. For now it checks your username and token: it joins Wi-Fi, opening the Wi-Fi screen if it isn't already connected, asks BGG about your user, and turns the radio off again. The line above it says how it went, such as `TOKEN OK FOR ALICE`. `BACK` while it's asking stops it first.
- Tap a game to see its details. `BACK` returns to the same page of the list.
- `BACK` on the list returns to the APPS page.

## Saves

Nothing is saved yet.
