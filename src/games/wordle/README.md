# Wordle

Six guesses for one five-letter word, the same word as the New York Times' Wordle that day. `GET PUZZLES` downloads every past answer and the list of accepted guesses, and after that playing never touches the network. There's no free play: a day is a day's word.

- **Menu:** today's date is the way into today's game. Below it are how today went, the record, a chart of guesses per win, `HOW TO PLAY`, `ARCHIVE` and `GET PUZZLES` (`ALL CAUGHT UP` when today is on the card).
- **The record** only counts games finished on their own date. The streak runs from today, or from yesterday while today is still open. A past day played from the archive is marked on the calendar and counts for nothing else. The first finish of a day is the one kept.
- **Marks:** solid black is the right letter in the right spot, a heavy border and a dot is the right letter in the wrong spot, and grey isn't in the word.
- **"Today"** is the device's local date, in the time zone chosen in Settings. With no clock, the newest puzzle on the card stands in.

**Saves:** all of these are in `/.pocketplay/`.

| Name | Kind | Holds |
|---|---|---|
| `wordle` | Save | The day on screen and the row being typed, so a wake returns to it. Erased on leaving the game for the menu or the archive. |
| `wordle-answers` | Blob | Five bytes per day from 2021-06-19, `.` for a day not known. |
| `wordle-words` | Blob | The accepted guesses, sorted, five bytes each, binary-searched in memory. |
| `wordle-results` | Blob | One byte per day: 1-6 solved, 7 lost, 8 started, with 0x80 set when finished on the day. |
| `wordle-games` | Blob | `day ANSWER GUESS GUESS...`, one line per day played. |

**Where the puzzles come from:** [mfilej/wrdl](https://github.com/mfilej/wrdl), a mirror that lands a day behind, then NYT's own `svc/wordle/v2/YYYY-MM-DD.json` for the days after it: at most 14 requests, and never for a date after today. The words are the New York Times'. This firmware ships none of them, and the device fetches them when asked.

**Copied code:** `wordle_rules.h` and `wordle_rules.cpp` are CrossPlay's `src/apps_local/wordle/WordleCore.h` and `.cpp`, changed only in one include and one comment, and `tests/wordle/test_wordle.cpp` is its test. The screens and game logic are ported from `WordleScreens.cpp`, `WordleActivity.cpp` and the calendar in `ConnectionsScreens.cpp`, redrawn without CrossPlay's UI toolkit. All of it matched CrossPlay commit `fbeac762` when last compared, and it's under the MIT licence, in `LICENSE-CrossPlay.txt`.

**Differences from CrossPlay:** the calendar and download screens use Jersey 25, because this firmware has no serif font. The download runs in the background, so `BACK` can stop it. A half-typed row survives sleep. Progress isn't imported from a CrossPlay card.
