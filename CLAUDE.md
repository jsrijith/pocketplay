# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

PocketPlay is a games-only firmware for the Seeed reTerminal Sticky (ESP32-S3), built up one step at a time so its owner can learn how ESP32 firmware works. Keep each step small, and explain the hardware reasoning.

## Commands

Run these from WSL. PlatformIO isn't on PATH.

```bash
./scripts/package.sh                      # build and write dist/pocketplay-full.bin
~/.platformio/penv/bin/pio run -e sticky  # build only
```

```bash
scripts/check.sh          # every PC test; only "CHECK-VERDICT: green" is a pass
tests/wordle/run.sh       # Wordle's rules and save packing
tests/tictactoe/run.sh    # Tic Tac Toe's rules
tests/settings/run.sh     # settings packing, including files from older firmware
tests/home/run.sh         # the home page's paging, the battery icon's fill and when the battery redraws
tests/dungeon/run.sh      # D&Diagrams: rules, all 65 puzzles, guide and save packing
tests/connections/run.sh  # Connections: rules, pack, import, record, save packing, and every archive board's tile fit
tests/solitaire/run.sh    # Solitaire: rules, save packing with the undo history, and the record
tests/minesweeper/run.sh  # Minesweeper: rules, flow and save packing
tests/sudoku/run.sh       # Sudoku: generator, solver, game and save packing
tests/knucklebones/run.sh # Knucklebones: rules, opponent, flow and save packing
tests/connectfour/run.sh  # Connect Four: rules, opponent, flow and save packing
tests/suguru/run.sh       # Suguru: rules, every bank puzzle's one solution, flow and save packing
tests/sokoban/run.sh      # Sokoban: every level, rules, flow and save packing
tests/queens/run.sh       # Queens: rules, every bank puzzle's one solution, flow and save packing
tests/symboldice/run.sh   # Symbol Dice: rolls, turns and save packing
tests/pegsolitaire/run.sh # Peg Solitaire: rules, whole games, flow and save packing
tests/textadv/run.sh      # Text Adventures: the Z-machine playing Zork, CZECH and Praxix, and the screen's text
tests/wordladder/run.sh   # Word Ladder: every stored ladder, days, play, hints, rows and save packing
tests/diceroller/run.sh   # Dice Roller: fair faces, rejection, totals and save packing
tests/nonogram/run.sh     # Nonograms: every bank puzzle line-solvable, rules, flow and save packing
tests/kakuro/run.sh       # Kakuro: every bank puzzle solved by run reasoning, rules, flow and save packing
```

Every firmware build runs `scripts/check.sh` first and stops if it fails (`scripts/pre_build_gate.py`). Tests compile with AddressSanitizer and UBSan, so a memory error stops them. To add a test, put `test_<name>.cpp` in `tests/<folder>/`, then compile and run it from that folder's `run.sh`, which takes its flags from `tests/flags.sh`. The check fails if `run.sh` doesn't mention a test file. The suites run at the same time, one per core, so a `run.sh` builds in its own `mktemp -d` folder and writes nothing outside it.

`tools/preview/run.sh` draws a game's screens on the PC as PNGs, to compare with CrossPlay's screenshots before flashing (`tools/preview/README.md`).

**Versions:** `VERSION` holds the version number, such as `0.1.0`; raise it by hand for a release. `scripts/version.py` runs before each build and adds the build (git's commit count) and the commit, with a `+` if there were uncommitted changes, as defines that `core/version.h` reads. The boot screen, the sleep screen and Settings' About page show them, and the serial log prints them at boot. The PC tests and the preview don't run the script, so they show `DEV`.

There are no linters. These tests cover code with no hardware in it, and a build only proves the rest compiles, so behaviour has to be confirmed on the device.

**Flashing is done by the user, from Windows.** WSL has no USB access to the device. Give the user commands to run in PowerShell, after they copy the image to `C:\Users\Srijith\Downloads`:

```powershell
python -m esptool --chip esp32s3 --port COM8 --baud 460800 write-flash 0x0 pocketplay-full.bin
python -m serial.tools.miniterm COM8 115200
```

Use `python`, because this machine has no `py` launcher. Always flash the merged image at `0x0`, never the app alone: the merged image includes `boot_app0.bin`, which selects app0 if an earlier firmware last booted from app1.

`gh` exists only as `gh.exe` on the Windows side. WSL has no stored GitHub credentials, so a push needs `-c credential.helper= -c 'credential.helper=!gh.exe auth git-credential'`.

## Architecture

`src/main.cpp` is an Arduino `setup()`/`loop()` program that runs the hardware. Everything else sits in a folder, and each folder may only use the ones listed for it:

| Folder | Holds | May use |
|---|---|---|
| `src/shell/` | Which screen is open, where taps go, and the home page | Everything below |
| `src/games/<name>/` | One game | `core/`, `ui/`, `games/game.h` |
| `src/apps/<name>/` | One app (none yet) | `core/`, `ui/`, its contract |
| `src/settings/` | The Settings, Wi-Fi, time zone and About screens, and their saved values | `core/`, `ui/` |
| `src/ui/` | Shared buttons, header, keyboard and logo | `core/` |
| `src/core/` | Drawing, text and fonts, save files, the radio, downloads and the clock | The SDK only |

Never include a file from another game or app, or from a higher row. Include a file in the same folder by its bare name, and anything else by its path from `src/`, such as `core/text.h`, never with `../`. Shared files are where branches conflict, so a game's change should stay in its own folder: `src/games/README.md` has the steps and rules.

It runs on top of eight `freeink-sdk` libraries (BatteryMonitor, BoardConfig, EInkDisplay, InputManager, PowerManager, Rtc, SDCardManager, SecureNet), which are symlinked through `lib_deps` in `platformio.ini`. `-DFREEINK_DEVICE_STICKY=1` makes the SDK's `BoardConfig::ACTIVE` resolve to the Sticky's pins and panel settings, so take pins from `ACTIVE` rather than hardcoding them. To use another SDK library (touch, input, sensors), add it to `lib_deps`. Its API is in `freeink-sdk/libs/*/*/include/`.

`freeink-sdk/` is a submodule pinned to the commit CrossPlay builds against. Don't edit it.

**Drawing:** code writes into the framebuffer from `display.getFrameBuffer()`, then `displayBufferAsync()` sends it to the panel. The framebuffer is 48,000 bytes in native 800x480 landscape: 1 bit per pixel, MSB first, 1 = white. There is no graphics library; `src/core/graphics.cpp` has the pixel and rectangle functions.

**Text:** `src/core/text.cpp` draws Jersey 25 from the headers in `src/core/fonts/`, which are copied unchanged from CrossPlay. Read `src/core/fonts/README.md` before replacing one, because regenerating them with newer tools moves text on every screen. Keep fonts 1-bit: this panel draws pure black and white, and a pixel font has no grey edges to lose. Take line height and capital height from `text.h`, never as numbers copied from a font, and centre labels with `drawTextInBox()`, because centring on the line box makes capitals hang low. Text is ASCII, and any other byte draws as an outlined box. The font headers define `static` data, so include them only from `text.cpp`, or each file that includes them gets its own copy.

**Orientation:** the firmware draws in portrait, 480 wide by 800 tall, held with the USB edge on the right. `setPixel()` is the only drawing code that knows the panel's native landscape layout, and `touchToScreenX()` and `touchToScreenY()` do the same for touch. A game whose `landscape()` is true is drawn 800 by 480 in the panel's own layout, with the USB edge at the top, as CrossPlay's Solitaire is. `shell::draw()` sets the orientation before every draw, and touch follows `shell::landscape()`, so nothing else sets it. Use `screenWidth()` and `screenHeight()` in code that runs in both, because `SCREEN_WIDTH` and `SCREEN_HEIGHT` are the portrait sizes. The sleep screen is always portrait.

**Boot and sleep screens:** `ui::drawSplash()` draws the logo (`ui/logo.cpp`, a die in a stitched pocket), the name, a line and the version. Boot shows it with a full refresh for `SPLASH_MS`, then the open screen with another full refresh; sleep leaves it on the panel with `PRESS OK TO WAKE`.

**Refreshing:** `displayBufferAsync()` returns once the frame is sent, so `loop()` keeps reading buttons while the panel updates. It allocates one 48 KB shadow frame on first use and keeps it. Once async refreshes have started, don't call a blocking `displayBuffer(FAST_REFRESH)`: in single-buffer mode it compares against the panel's stored previous frame, which async refreshes leave out of date. To wait for a frame, call `displayBufferAsync()` and then `waitRefreshComplete()`. Refreshing is counted, not timed: boot and `RedrawFull` use a full refresh, and every Nth other update is a half refresh to clear ghosts, where N is the `REFRESH` setting and defaults to 30. Nothing redraws while the device is idle except the clock on the Wi-Fi status page and the battery on the home page, so a flash on an untouched game screen means a restart, not a refresh.

**Battery:** `core/battery.cpp` reads the BQ27220 fuel gauge through the SDK's `BatteryMonitor`, on the same `Wire1` bus as the clock chip, so keep its calls in `loop()`. The home page reads it on every draw, and checks it every 5 seconds while open. It redraws when charging starts or stops, or the percent moves while charging, once a change has shown on two checks in a row (`core/battery_state.h`). A falling percent on battery doesn't redraw. `charging` comes from the direction of the battery's current, so it goes false once the battery is full, even with the cable in: this board has no simple signal for a connected cable. Don't write to the gauge.

**Input:** buttons and touch both go through the SDK's `InputManager`, read in `main.cpp` and passed to `shell.cpp` as screen positions. Call `update()` once per loop, then read its events; they only last until the next `update()`. On this board it doesn't debounce buttons, so the 15 ms loop is the debounce: polling much faster can count one press twice. OK is read as `BTN_POWER`, because `setSharedConfirmPowerShortPressEmitsPower(true)` makes it follow the button straight away instead of reporting `BTN_CONFIRM` on release. Touch positions arrive as 0..1 in the panel's landscape frame. A game whose `usesHold()` is true also gets `onPress`, `onHold` (after `HOLD_MS`, 400 ms, without moving past the SDK's tap slop) and `onRelease`, and a hold calls `suppressTouchContact()` so its lift doesn't tap as well. Every other screen only gets taps, so a slow tap still taps there. A game also gets `onSwipe()` with a swipe's start and end (the SDK's: 60 px within 700 ms), which is never also a tap, and every other screen ignores swipes. The Up and Down buttons step the home page, the Wi-Fi list and the zone list, and in a game they go to `onUpPressed()` and `onDownPressed()`, which do nothing unless the game uses them, as Connect Four's how to play does.

**Games:** each game lives in `src/games/<name>/`, implements `Game` from `src/games/game.h` as a static object, and has one line in `src/games/game_list.inc`. Keep rules and save packing free of screen and card code, so they can be tested off the device, as `tictactoe_rules.h` and `tictactoe_save.cpp` are. A tap that changes nothing must return `TapResult::Nothing`, because a blink that changes nothing looks like a bug. Use `RedrawFull` when a game starts or ends, and `Redraw` for a move. A game that needs Wi-Fi returns `TapResult::OpenWifi`, and `onWifiClosed()` runs when the Wi-Fi screen closes, which happens by itself once it joins. A game with work between taps overrides `update()`, and `busy()` if a sleep would throw that work away. `shell.cpp` keeps the open game's name in `session.sav`, so a wake reopens it. Don't change a game's `name()` casually, and never store a position in the list instead of the name.

**Storage:** saves live on the SD card in `/.pocketplay/`, one file per name. `storage.cpp` knows about files and nothing about games, and `state.cpp` turns values into bytes and nothing else, so it can be tested off the device. Follow these rules for anything new that gets saved:

- Save on every change, not only before sleep, because a crash or flat battery never reaches the sleep path.
- Pack values byte by byte instead of copying a struct, and raise the version when the layout changes.
- Have `unpack` reject out-of-range values, and fall back to defaults rather than a half-restored game.
- Erase a game's save when the game ends, or it gets offered for resume.

`storage::save()` writes a temp file, reads it back, then replaces the old save, and `load()` falls back to a valid temp file. Don't shortcut this: a power cut can come at any step, and opening a file for writing empties it. Saves hold up to 512 bytes. For more, `saveBlob()` and `loadBlob()` work the same way, with the data in PSRAM. Files the player copies onto the card, such as story files, are read with `listFolder()` and `readFile()`, which never write: keep them outside `/.pocketplay/`.

**One task for the bus:** the card and panel share an SPI bus, with nothing guarding it. That's safe because everything that uses them runs in `loop()` and a refresh only watches the busy pin. The Wi-Fi stack runs its own tasks, but they never touch the card or panel. Keep all `WiFi` calls in `loop()` too. A second task that touches the card or panel needs a lock first.

**Downloads:** `core/download.cpp` is the one exception to everything running in `loop()`, because TLS would otherwise stop input for a minute. Its task uses the network and hands bytes to a sink, which must only store them in memory. `loop()` writes them to the card after the download has ended. Don't turn the radio off while one is running: call `requestCancel()` and wait for `state()` to change. HTTPS uses wolfSSL, and `scripts/patch_wolfssl.py` patches its settings before each build. Certificates aren't checked, so never send anything private this way.

**Clock:** the system clock and the PCF8563 clock chip both keep UTC, and `localtime_r()` applies the zone chosen in Settings. Use `localtime_r()` for anything the player sees as a date. `core/clock.cpp` sets the system clock from the chip at boot, and sets the chip when a time server answers. Never call `configTime()`, which replaces the zone. The zone list is saved by position, so only add to its end.

**Wi-Fi:** `network.cpp` owns the radio and knows nothing about the screen or card, and `wifi_screen.cpp` holds the screens and remembered networks. Follow these rules:

- The radio stays off unless it's scanning or using a network. Games must turn it on only when needed, because a running radio drains the battery.
- Don't wait in `loop()` for the network. Start something, then check on it from `network::update()`.
- Keep `WiFi.persistent(false)`, or the driver stores its own plain copy of the password in flash.
- Only remember a network after it has joined, and never log a password.
- `download::start()` checks `network::state()` is `Connected`, because entering the socket layer with Wi-Fi never started crashes the device instead of returning an error. Anything else using sockets must check too.
- A wrong password usually doesn't report a failure, because the driver keeps retrying. The join time limit is what ends the attempt.
- `configTime()` holds the network stack's lock while it changes SNTP, so stopping SNTP holds it too. `stopTimeCheck()` only runs after `startTimeCheck()`, so the stack is up by then.
- ESP-NOW multiplayer can't run alongside a Wi-Fi connection, so it has to take over the radio when it's added.

## Hardware rules

These cause silent failures, and the build won't catch them:

- **Boot order in `setup()` matters.** `BoardConfig::holdPowerRails()` runs first, then `BoardConfig::releaseSdRail()`, then input, then `storage::begin()`, then `display.begin()`. The SD card shares the panel's SPI bus, and an unpowered card clamps the bus so the panel never hears a command.
- **`gpio_hold_en` survives resets and reflashing.** A held pin silently ignores writes, so call `gpio_hold_dis` before driving any pin that a sleep path, including another firmware's, may have held.
- **Sleep cuts the card's power.** The SDK's `shutdown()` does nothing on this board, so a save has to finish before the rails go down. `storage::save()` reads its file back, which makes the card finish first.
- **Sleep order in `goToSleep()` matters.** The radio goes off first. `display.deepSleep()` runs next while the panel still has power, then the latches on GPIO45/46 are held HIGH, then the rails are cut, then the chip sleeps.
- **Waking is a full chip reset.** Every wake runs `setup()` again. Only the OK button (GPIO4) can wake the device, since touch power is cut during sleep. OK must be released once after boot before a hold counts as sleep.
- **USB-C reaches UART0 through a CH343 chip, not native USB.** That's why `ARDUINO_USB_CDC_ON_BOOT=0` is set, which differs from CrossPlay. Opening the serial port resets the device.
- **GPIO0 is a strapping pin** and also the SCL of the sensor bus the clock chip is on. Don't leave a pull on it that disturbs boot mode.
- **Keep the device recoverable.** Never write eFuses or enable secure boot or flash encryption, since those can permanently lock the chip.

## Reference

`/home/srijith/mycrossplay` is a working full firmware for this device, built on the same SDK. Read it to see how something is done on the Sticky, but don't modify it or copy its structure wholesale. Its `CLAUDE.md` describes that project and doesn't apply here.

**Copying a game from CrossPlay:** its games are in `src/apps_local/<name>/`, and `src/games/wordle/` shows the result.

- Copy the rules (`<Name>Core.h/.cpp`) and their test from `host-tests/<name>/` unchanged apart from includes, and keep the MIT licence beside them. The test goes in `tests/<name>/`.
- Rewrite the screens and activity on `core/` and `ui/`, because CrossPlay's Activity system, FreeInkUI and Toybox aren't here. Keep the features and behaviour the same.
- List every difference from CrossPlay for the user before building. The game's `README.md` is for players (rules, getting around, saves) and only credits CrossPlay in one line, not the differences.
- Before calling reported behaviour a bug, compare it with CrossPlay. If the two match, tell the user that rather than changing the rule.
