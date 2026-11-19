# PocketBGG

Your BoardGameGeek collection, to browse on the device.

**Work in progress.** It syncs and keeps your collection; thumbnails and filters are still to come.

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

- The list shows each game's name, with the year, how many times it's been played and its status under it, such as `OWNED` or `WISHLIST`. Long names wrap onto a second line rather than being cut short.
- The Up and Down buttons turn the pages. `PAGE 1 OF 25` at the top says where you are, and the count on the left how many games there are.
- The list is kept on the card, so it's there without Wi-Fi, after sleep and after a restart. The line above `SYNC` says when it was last synced, in the time zone chosen in Settings.
- `SYNC` downloads your whole collection from BGG: every game with any status (owned, wishlist, previously owned and the rest), expansions included. It joins Wi-Fi, opening the Wi-Fi screen if it isn't already connected, and turns the radio off again when it's done. BGG often answers "preparing it" first, so it waits 5 seconds and asks again, up to 6 times. The line above it says how it's going. `BACK` while it's downloading stops it first.
- Tap a game to see its details, with its player count and play time. `BACK` returns to the same page of the list.
- `BACK` on the list returns to the APPS page.

## Saves

All in `/.pocketplay/`:

| Name | Holds |
|---|---|
| `pocketbgg-list` | The collection as it was last synced, and when. It's replaced only once a new sync has fully arrived and been read, so a failed sync keeps the old list. About 200 bytes a game. |

Your username and token stay in `/pocketbgg/config.txt`, which PocketBGG only ever reads.
