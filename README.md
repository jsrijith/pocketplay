# PocketPlay

A pocket games console for the [Seeed reTerminal Sticky](https://www.seeedstudio.com/), an e-ink device with a touch screen. PocketPlay replaces the device's firmware with a home page of 29 games, from daily word puzzles to logic puzzles, card games and text adventures. Everything is played by tapping the screen, and everything is designed for e-ink: black and white, few refreshes, and nothing that moves unless you touch it.

PocketPlay drew heavy inspiration from [CrossPlay](https://github.com/ma-r-s/crossplay), and while it has since grown to include its own unique design, games, and features, most of its games were ported from two open-source projects: nine from CrossPlay, including the very first ones, and ten from [touch-licorice-apps](https://github.com/omer-faruq-xteink/touch-licorice-apps) by Omer Faruk Inel. I would like to extend my sincere thanks to the authors of both: CrossPlay laid the foundation that made this possible, and touch-licorice-apps brought many of the puzzle games and their puzzles.

This entire project was also developed using Claude. Having no background in firmware development or C++, I could not have built this independently—Claude was instrumental in turning the idea into reality.

I have also credited all the open-source projects referenced here. If I have missed any attribution, please feel free to reach out and let me know.

**Games included:**

- **Wordle:** the daily puzzle and every past day, with a calendar archive, your record and a chart of your guesses. [How it plays](src/games/wordle/README.md)
- **Tic Tac Toe:** two players on one device.
- **D&Diagrams:** 64 dungeon logic puzzles: place walls so the numbers, monsters and treasure all fit. [How it plays](src/games/dungeon/README.md)
- **Connections:** sixteen words in four hidden groups, the daily puzzle and every past one, with a calendar archive and your record. [How it plays](src/games/connections/README.md)
- **Solitaire:** Klondike, drawing one or three, with undo and your record. It plays in landscape, with the USB edge at the top. [How it plays](src/games/solitaire/README.md)
- **Minesweeper:** eight by ten with ten mines. Tap to dig, hold to flag, and the first dig is always safe. [How it plays](src/games/minesweeper/README.md)
- **Sudoku:** puzzles made on the device at four levels, graded by the hardest technique they need, with pencil marks, undo and hints. [How it plays](src/games/sudoku/README.md)
- **Knucklebones:** a dice game against the computer: drop each roll into a column, where matching dice multiply and knock out the opponent's. [How it plays](src/games/knucklebones/README.md)
- **Connect Four:** drop discs against the computer to get four in a line, across, up or on a diagonal. [How it plays](src/games/connectfour/README.md)
- **Lights Out:** a 5x5 board where each tap switches a square and its neighbours. Switch them all off, in as few presses as the target, with hint and undo. [How it plays](src/games/lightsout/README.md)
- **One Clue:** a deck of 110 word cards for the party game where everyone writes a one-word clue and matching clues cancel. It deals the cards; you keep score on paper. [How it plays](src/games/oneclue/README.md)
- **Suguru:** 800 logic puzzles from 5x5 to 8x8 in four banks. Fill each outlined region with 1 up to its size, and never let touching squares match. [How it plays](src/games/suguru/README.md)
- **Sokoban:** push every box onto a goal, in 385 levels from five classic packs. Swipe or tap beside the player to move, with undo and your best per level. [How it plays](src/games/sokoban/README.md)
- **Queens:** 1200 logic puzzles: one queen (or, in Star Battle, two stars) in every row, column and outlined region, none touching. Six sizes from 6x6 to 10x10, with hints. [How it plays](src/games/queens/README.md)
- **Symbol Dice:** two dice of six symbols for a 12-turn roll-and-write game played on paper, with a turn counter. [How it plays](src/games/symboldice/README.md)
- **Peg Solitaire:** jump pegs until as few as possible are left, on the 33-hole English board or the 15-hole triangle, with undo and your best per board. [How it plays](src/games/pegsolitaire/README.md)
- **Text Adventures:** classic interactive fiction, with Zork I, II and III built in and any `.z3`, `.z5` or `.z8` story file you copy to the card. Tap directions and the game's own words instead of typing. [How it plays](src/games/textadv/README.md)
- **Word Ladder:** a new ladder every day: change one word into another a letter at a time, every step a word, with par, hints and earlier days. [How it plays](src/games/wordladder/README.md)
- **Dice Roller:** two dice and their total, with one button to roll again, for any board game. [How it plays](src/games/diceroller/README.md)
- **Nonograms:** picture logic puzzles, filling cells to match the row and column clues, 600 of them in four sizes, with X marks, undo and half-done puzzles kept. [How it plays](src/games/nonogram/README.md)
- **Kakuro:** cross sums, filling digits so every run adds up to its clue with none repeated, 600 puzzles in three levels, with the ways to make each run shown and hints. [How it plays](src/games/kakuro/README.md)
- **Bingo Caller:** calls 30, 75, 80 or 90-ball bingo for players with paper cards, one random number a tap with no repeats, and a board of every number called. [How it plays](src/games/bingocaller/README.md)
- **Island Maps:** 55 official and fan-made island layouts for laying out the tiles of the Forbidden Island board game, stepped through with Up and Down or picked at random. [How it plays](src/games/islandmaps/README.md)
- **Think Alike:** a deck of questions for the party game where everyone writes an answer and matching the most common one scores. The questions are a text file on the card, so you can add your own. [How it plays](src/games/thinkalike/README.md)
- **Authority Tracker:** two players' scores on one screen, starting at 50, with buttons for ±1, ±5 and ±10. The game ends when a score reaches 0. [How it plays](src/games/authoritytracker/README.md)
- **Grid Gambit:** chess pieces on a 4x4 board, where every move must take a piece and one piece left wins. Puzzles are made on the device in four levels, with undo and a solved count per level. [How it plays](src/games/gridgambit/README.md)
- **Chess Puzzles:** 2398 tactics puzzles from the Lichess database in six banks, from mate in 1 to expert: find the best move and play the line out, with hints, show the move and your solved count per bank. [How it plays](src/games/chesspuzzles/README.md)
- **Polyomino Panic:** holds the decks for the paper game: deals a different starting shape to each player, then flips two shapes a round from the 40-card deck until it runs out. Plotting and scoring stay on paper. [How it plays](src/games/polyominopanic/README.md)
- **Crown & Crest:** the four dice for the two-player Crown & Crest, each with its own six faces of shields and marks: roll, then share the dice out (the roller takes one, the other player two, the roller the last). [How it plays](src/games/crowncrest/README.md)

A game in progress is saved on every move, so it's still there after the device sleeps or the battery runs out.

## Disclaimer

- **Built with AI assistance:** PocketPlay was written with Claude Code. I don't have a background in C++ or firmware engineering, so no expert has reviewed the code.
- **Tested on one device:** it has been tested only on the Seeed reTerminal Sticky, where every feature works as expected. It may not work, or may not be stable, on other hardware or board revisions.
- **No warranty:** flashing third-party firmware can leave a device unusable or make it behave unexpectedly. PocketPlay is provided as-is, without warranty of any kind (see [Licence](#licence)). Read the code, back up your current firmware ([step 3](#3-back-up-your-current-firmware)), and flash at your own risk.

## What you need

- A Seeed reTerminal Sticky
- A USB-C **data** cable (charge-only cables don't show up on the computer)
- A microSD card formatted as FAT32 or exFAT, to keep your saves and downloaded puzzles
- A Windows computer with Python 3, for flashing. That's the only setup that's been tested

## Install

### 1. Download the firmware

Download `pocketplay-full.bin` from the [latest release](https://github.com/jsrijith/pocketplay/releases/latest) into your Downloads folder. The release notes give its SHA-256 checksum. To check the file arrived intact, compare it with:

```powershell
Get-FileHash $HOME\Downloads\pocketplay-full.bin
```

To build the firmware yourself instead, see [Building the firmware](#building-the-firmware).

### 2. Set up flashing (in Windows PowerShell)

Flashing uses esptool, Espressif's flashing tool, from PowerShell. (If you built the firmware in WSL, flash from Windows anyway: WSL can't reach the device over USB.)

```powershell
python -m pip install --upgrade esptool
```

Plug in the device and look in Device Manager under "Ports (COM & LPT)" to find its port. The commands below use `COM8`, so change that to yours. If no port appears, try another cable, then install the WCH CH343 driver.

### 3. Back up your current firmware

Flashing replaces whatever is on the device, so save a copy first. You only need to do this once. It takes about 10 to 15 minutes, so keep the file somewhere safe.

```powershell
cd $HOME\Downloads
python -m esptool --chip esp32s3 --port COM8 flash-id
python -m esptool --chip esp32s3 --port COM8 --baud 230400 read-flash 0 0x1000000 sticky-backup.bin
```

The first command only checks the connection. If it fails, press OK to wake the device and try again.

### 4. Flash PocketPlay

```powershell
python -m esptool --chip esp32s3 --port COM8 --baud 460800 write-flash 0x0 pocketplay-full.bin
```

The device restarts into the PocketPlay home page. Always flash at `0x0`, because this one file includes everything the device needs to start.

### 5. First-time setup

1. Put in the SD card and restart the device. A card put in later is only seen after a restart.
2. Open **Settings** with the cog at the bottom right of the home page, and choose your **TIME ZONE**, so Wordle's "today" matches your date.
3. Open **Wordle** and tap **GET PUZZLES**. It joins Wi-Fi if needed, downloads the puzzles, then turns the radio off. After that, playing needs no network until you want new puzzles. **Connections** has its own **GET PUZZLES**, which works the same way.

## Using it

| To do this | Do this |
|---|---|
| Open a game or setting | Tap it |
| Open Settings | Tap the cog at the bottom right of the home page |
| Change page | Tap a page number at the bottom, or press Up or Down. The home page shows 10 games a page, and Down on the last page goes back to the first |
| See the battery | It's at the bottom left of the home page, with a bolt while charging |
| Sleep | Hold OK for 1 second, or leave it idle for the sleep time |
| Wake | Press OK. A game that was open comes back; otherwise you return to the home page |

The boot and sleep screens show the logo, with the version and build along the bottom, such as `VERSION 0.1.0  BUILD 57 (A1B2C3D)`. Quote that when reporting a problem.

**Settings:**

| Row | What it does |
|---|---|
| Wi-Fi | Joins the most recent network, or scans for one |
| Sleep | Idle time before sleeping: 1, 5 or 10 minutes |
| Time zone | Your local time zone |
| Refresh | How many screen updates pass before a half refresh clears ghost images: 5, 10, 15 or 30 (the default), or never |
| About | The version and build, how many games there are, and where the code lives |

**Good to know:**

- **Wi-Fi:** it joins 2.4 GHz networks only, not 5 GHz, enterprise or hidden ones. The radio is only on while it's needed, and sleep always turns it off. Up to 4 networks are remembered.
- **Passwords:** Wi-Fi passwords are saved on the SD card scrambled with the chip's MAC address. **This isn't encryption:** anyone with both the card and this code can undo it, so don't lend out a card you've used on a private network.
- **Downloads:** Wordle's and Connections' downloads don't verify the server's certificate. The device sends no personal data with them, but a network attacker could feed it wrong puzzles.
- **Saves:** they live in `/.pocketplay/` on the card, and your other files are left alone. Without a card the screen shows `NO CARD`, and everything else works but nothing survives sleep.

## Troubleshooting

| Problem | Try |
|---|---|
| No COM port appears | Use a data cable, then install the WCH CH343 driver |
| esptool can't connect | Press OK to wake the device, close anything else using the port, and try again |
| esptool doesn't recognise `write-flash` | Run `python -m pip install --upgrade esptool` |
| Wordle shows yesterday's date | Today's word didn't download, so tap **GET PUZZLES** again |
| A win didn't change the record | The record only counts a puzzle finished on its own date, so archive days don't add to it |

To see the device's logs, run this and press `Ctrl+]` to exit. Opening the port resets the device, so a redraw at that moment is normal.

```powershell
python -m serial.tools.miniterm COM8 115200
```

## Going back to your old firmware

Flash your backup the same way. Flashing doesn't touch the SD card, so your saves are still there if you come back.

```powershell
python -m esptool --chip esp32s3 --port COM8 --baud 460800 write-flash 0x0 sticky-backup.bin
```

## For developers

### Building the firmware

Building needs Linux, or WSL (Ubuntu) on Windows, which is what's been tested. The first build downloads its tools and takes several minutes.

Install the tools:

```bash
sudo apt install git python3 python3-venv g++
curl -fsSL -o get-platformio.py https://raw.githubusercontent.com/platformio/platformio-core-installer/master/get-platformio.py
python3 get-platformio.py
```

Get the code and build it:

```bash
git clone --recurse-submodules https://github.com/jsrijith/pocketplay.git
cd pocketplay
./scripts/package.sh
```

When it finishes, it prints `Ready to flash` and the image is at `dist/pocketplay-full.bin`. To flash it from Windows, copy it somewhere Windows can reach, replacing `<you>` with your Windows user name:

```bash
cp dist/pocketplay-full.bin /mnt/c/Users/<you>/Downloads/
```

Then flash it as in [Install](#install), from step 2.

### Tests and versions

Every firmware build runs the PC tests first and stops if any fail. You can also run them on their own:

```bash
scripts/check.sh    # every test; only "CHECK-VERDICT: green" is a pass
```

To add a game, follow [src/games/README.md](src/games/README.md). A game is one folder and one line in `game_list.inc`.

**Versions:** `VERSION` holds the number, in [semantic versioning](https://semver.org): a release with fixes only raises the last part (0.2.0 to 0.2.1), new features the middle (0.2.1 to 0.3.0), and a breaking change, such as saves that no longer load, the first (from 1.0 on). `scripts/release.sh` reads the `fix:` and `feat:` commits since the last release, raises `VERSION` to match, commits and tags it; `--dry-run` only says what it would do. Before a release, write what changed for players under `## Unreleased` in [CHANGELOG.md](CHANGELOG.md), which the release notes take their "What's new" from. Pushing the tag makes GitHub build the image from it and create a draft release, which is published from the Releases page. Every push to `main` is also built and tested on GitHub. Each build adds the build number (the commit count) and the commit, with a `+` if it had uncommitted changes.

| Path | Purpose |
|---|---|
| `src/main.cpp` | Boot, input, refreshing the panel, sleep |
| `src/shell/` | Which screen is open, where taps go, and the home page |
| `src/games/` | One folder per game, plus what they share |
| `src/apps/` | One folder per app, when there are some. [The plan](src/apps/README.md) |
| `src/settings/` | The Settings, Wi-Fi, time zone and About screens, and their saved values |
| `src/ui/` | The buttons, header, keyboard and logo that screens share |
| `src/core/` | Drawing, text and fonts, save files, the radio, downloads and the clock |
| `tests/` | Tests that run on a PC |
| `freeink-sdk/` | Screen, input and power libraries, as a git submodule |

**The hardware:**

| Part | Details |
|---|---|
| Chip | ESP32-S3: dual-core, 16MB flash, 8MB PSRAM |
| Screen | 3.97" 800x480 black and white e-ink, SSD1677 controller, SPI |
| USB-C | Goes through a CH343 USB-serial chip to UART0, not the S3's native USB |
| Buttons | OK/Power on GPIO4, Up on GPIO5, Down on GPIO6, all active low |
| Touch | GT911 over I2C: SDA GPIO3, SCL GPIO2, INT GPIO21, RST GPIO41, power enable GPIO42 |
| SD card | SPI, shared with the screen: SCK GPIO13, MISO GPIO12, MOSI GPIO14, CS GPIO8, power enable GPIO10 |

**How it starts:**

1. The ROM bootloader, built into the chip, loads the second-stage bootloader from flash at `0x0`.
2. That reads the partition table at `0x8000`, then `otadata` at `0xe000` to pick an app slot.
3. The app starts from `0x10000`. Arduino starts FreeRTOS, then calls `setup()` once and `loop()` repeatedly.
4. `setup()` holds the power latches, powers the SD rail, starts input, loads the saves from the card, starts the panel, shows the boot screen, and draws.

Deep sleep turns almost everything off, so waking runs all of this from the start.

## Credits

PocketPlay builds on other people's work, each under its own licence, kept beside the code it covers:

- [CrossPlay](https://github.com/ma-r-s/crossplay) by Mario Ruiz: the project that inspired PocketPlay. Several games began as ports of its games, and the font headers come from it (MIT)
- [touch-licorice-apps](https://github.com/omer-faruq-xteink/touch-licorice-apps) by Omer Faruk Inel: the rules and puzzles behind Lights Out, Suguru, Sokoban, Queens, Peg Solitaire, Text Adventures, Word Ladder, Nonograms, Kakuro and Chess Puzzles (MIT)
- [Lichess puzzle database](https://database.lichess.org/#puzzles): Chess Puzzles' puzzles (CC0)
- [Lucide](https://lucide.dev): home-page icons, and Grid Gambit's pieces (ISC)
- [FreeInk SDK](https://github.com/ma-r-s/freeink-sdk): hardware libraries (MIT)
- [wolfSSL](https://www.wolfssl.com/): HTTPS (GPLv2 or later, or commercial)
- [SdFat](https://github.com/greiman/SdFat): SD card access (MIT)
- Jersey 25: the font (SIL Open Font License)
- Noto Sans Symbols 2: Solitaire's suit marks, drawn from its card-suit glyphs (SIL Open Font License, in `src/games/solitaire/OFL-Noto.txt`)

Zork I, II and III are © Microsoft, released under the MIT licence. Sokoban's level packs are by David W. Skinner and Aymeric du Peloux, published with their permission. Word Ladder's word lists come from Alan Beale's 12dicts and Kevin Atkinson's AGID.

Island Maps' layouts are the official islands of Forbidden Island, a game by Matt Leacock published by Gamewright, and fan-made islands by the players named in [its README](src/games/islandmaps/README.md). This project isn't affiliated with either.

Wordle and Connections are trademarks of The New York Times Company, and this project isn't affiliated with it. The firmware ships no puzzles: the device downloads them when you ask, from [mfilej/wrdl](https://github.com/mfilej/wrdl), the New York Times and [Eyefyre/NYT-Connections-Answers](https://github.com/Eyefyre/NYT-Connections-Answers). The Connections test fixture in `tests/connections/` holds a copy of that archive and is never built into the firmware.

## Licence

PocketPlay's own code is under the MIT licence (`LICENSE`). Code and data copied from other projects keep their own licences, in the `LICENSE-*.txt` and `OFL-*.txt` files beside them, as listed under Credits.

The firmware links wolfSSL, which is under the GPL (version 2 or later), so a built firmware image as a whole is bound by the GPL: anyone who passes one on must also offer its source, which this repository is.
