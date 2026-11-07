# Wordle

Guess the day's hidden five-letter word in six tries. It's the same word as the New York Times' Wordle for that date, and every past day can be played too.

## Rules

- Type a five-letter word and tap the tick key. It has to be a real word from the accepted list, or you'll see `Not in the word list.` and can change it.
- After each guess, every letter is marked:
  - **Solid black:** the right letter in the right place.
  - **Heavy border with a dot:** the letter is in the word, but somewhere else.
  - **Grey:** the letter isn't in the word.
- Find the word within six guesses to win. After the sixth wrong guess the game is lost and the word is shown.
- There's no free play: each day has exactly one word.

## Getting started

The device has no words until you fetch them. On the menu, tap `GET PUZZLES`. It joins Wi-Fi if it needs to, downloads every past answer and the list of accepted guesses, and turns the radio off again. `BACK` stops a download. After that, playing never uses the network. Tap `GET PUZZLES` again on a later day to get the new words; it reads `ALL CAUGHT UP` when today's word is already on the card.

"Today" is the device's local date, in the time zone chosen in Settings.

## Getting around

- **Menu:** tap today's date to play today's word. Below it are how today went, your record, a chart of how many guesses your wins took, and `HOW TO PLAY`, `ARCHIVE` and `GET PUZZLES`.
- **Game:** tap the on-screen keyboard to type. The tick key at the left of the bottom row enters the guess (it's grey until five letters are typed), and the delete key at the right removes a letter. Letters on the keyboard take the marks they've earned, so you can see which ones are left. When the game ends, `NEXT` opens the next day you haven't finished, and `ARCHIVE` opens the calendar.
- **Archive:** a calendar, one month at a time. Use the `YEAR` and `MONTH` steppers to move, and `TODAY` to jump back to the newest month. Tap a day to play it. Days you've played show how they went.
- **How to play:** a short reminder of the rules.
- `BACK` in the header goes from the game, archive or how to play to the menu, and from the menu to the home page.

## Your record

- Only games finished on their own date count towards the record and the streak. A past day played from the archive is marked on the calendar but doesn't change the record.
- The streak runs back from today, or from yesterday while today is still unplayed.
- The first time you finish a day is the result that's kept. Playing it again doesn't change it.

## Saves

All in `/.pocketplay/`:

| Name | Holds |
|---|---|
| `wordle` | The day on screen and the half-typed row, so waking returns to it. Erased when you leave for the menu or archive. |
| `wordle-answers` | Every downloaded answer, one per day. |
| `wordle-words` | The accepted guesses. |
| `wordle-results` | How each day went. |
| `wordle-games` | The guesses made on each day played. |

## Credits

Copied from CrossPlay and modified for PocketPlay, under the MIT licence (`LICENSE-CrossPlay.txt`). The icon is from Lucide (ISC licence). The words are the New York Times', fetched from mfilej/wrdl and NYT.
