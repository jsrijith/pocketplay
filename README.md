# BoardByte

A games firmware for the Seeed reTerminal Sticky, built up one step at a time.
It opens on a home page listing the games, with Settings at the bottom. Everything is tapped on the touch screen.

- **Games:** each one describes itself in its own folder under `src/games/`.
- **Settings:** the Wi-Fi row joins the most recent network, or scans for one if none is remembered. Once connected, the status screen shows the IP address, signal strength and UTC time. The sleep row chooses 1, 5 or 10 minutes.
- **Hold OK for 1 second, or leave it idle for the sleep time:** sleep. The screen shows `ASLEEP`.
- **Press OK while asleep:** the chip restarts. A game that was open comes back, and anything else returns to the home page.

The Wi-Fi radio is only on while scanning or connected, and sleep always turns it off. Up to 4 networks are remembered, and only after they've joined successfully. It can't join 5 GHz, enterprise or hidden networks, because the chip only has 2.4 GHz and the list leaves the other two out.

Passwords are saved on the card scrambled with the chip's MAC address. **This isn't encryption:** it stops someone reading a password straight off the card, but anyone with the card and this code can undo it.

Saving needs a FAT32 or exFAT SD card, and saves go in `/.boardbyte/`, leaving other files alone. Without a card the screen shows `NO CARD` and everything else still works, though nothing survives sleep. A card put in later is only seen after a restart.

Upgrading from an earlier version leaves an unused `/.boardbyte/game.sav` on the card. It's harmless, and you can delete it from a PC.

## The hardware

| Part | Details |
|---|---|
| Chip | ESP32-S3: dual-core, 16MB flash, 8MB PSRAM |
| Screen | 3.97" 800x480 black and white e-ink, SSD1677 controller, SPI |
| USB-C | Goes through a CH343 USB-serial chip to UART0, not the S3's native USB |
| Buttons | OK/Power on GPIO4, Up on GPIO5, Down on GPIO6, all active low |
| Touch | GT911 over I2C: SDA GPIO3, SCL GPIO2, INT GPIO21, RST GPIO41, power enable GPIO42 |
| SD card | SPI, shared with the screen: SCK GPIO13, MISO GPIO12, MOSI GPIO14, CS GPIO8, power enable GPIO10 |

## Folders

| Path | Purpose |
|---|---|
| `src/main.cpp` | Boot, input, refreshing the panel, sleep |
| `src/shell/` | Which screen is open, where taps go, which game to reopen, and the home page |
| `src/games/` | One folder per game, plus what they all share. [How to add a game](src/games/README.md) |
| `src/apps/` | One folder per app, when there are some. [The plan](src/apps/README.md) |
| `src/settings/` | The Settings and Wi-Fi screens, and how their values turn into bytes |
| `src/ui/` | The buttons, header and keyboard that screens share |
| `src/core/` | Pixels and the portrait mapping, text and fonts, save files, and the Wi-Fi radio |
| `freeink-sdk/` | Screen and power libraries, as a git submodule |
| `scripts/package.sh` | Builds and produces `dist/boardbyte-full.bin` |
| `platformio.ini`, `partitions.csv` | Build settings and how the 16MB flash is divided |

After cloning, run `git submodule update --init freeink-sdk`.

## How it boots

1. The ROM bootloader runs. It's built into the chip and can't be overwritten.
2. It loads the second-stage bootloader from flash at `0x0`.
3. That reads the partition table at `0x8000`, then `otadata` at `0xe000` to pick an app slot.
4. The app starts from `0x10000`. Arduino starts FreeRTOS, then calls `setup()` once and `loop()` repeatedly.
5. `setup()` holds the power latches, powers the SD rail, starts input, loads the saves from the card, starts the panel and draws.

Deep sleep turns almost everything off, so waking runs all of this from the start.

## Tools

**In WSL:** PlatformIO, at `~/.platformio/penv/bin/pio`.

**On Windows:**

- Python 3, then `python -m pip install esptool`.
- A USB-C data cable. Charge-only cables don't show a COM port.
- The WCH CH343 driver, only if no COM port appears when you plug in.

## Build, in WSL

```bash
./scripts/package.sh
cp dist/boardbyte-full.bin /mnt/c/Users/<you>/Downloads/
```

## Flash, in Windows PowerShell

Find the port in Device Manager under "Ports (COM & LPT)". These examples use `COM8`.

```powershell
cd $HOME\Downloads

# 1. Check the connection. This only reads.
python -m esptool --chip esp32s3 --port COM8 flash-id

# 2. Back up what's on the device. Do this once, and keep the file safe.
#    It takes roughly 10 to 15 minutes.
python -m esptool --chip esp32s3 --port COM8 --baud 230400 read-flash 0 0x1000000 sticky-backup.bin

# 3. Flash.
python -m esptool --chip esp32s3 --port COM8 --baud 460800 write-flash 0x0 boardbyte-full.bin
```

If the connection fails, press OK to wake the device and try again.

## See the logs

```powershell
python -m serial.tools.miniterm COM8 115200
```

Press `Ctrl+]` to exit. Opening the port resets the device, so a redraw at that moment is normal.

## Going back to another firmware

Flash over this one the same way, as often as you like. Flashing doesn't touch the SD card, so your saves are still there if you come back.

```powershell
# Your backup
python -m esptool --chip esp32s3 --port COM8 --baud 460800 write-flash 0x0 sticky-backup.bin

# A CrossPlay release
python -m esptool --chip esp32s3 --port COM8 --baud 460800 write-flash 0x0 crossplay-<version>-sticky-full.bin
```

To use your own CrossPlay build, flash `.pio/build/gh_release_sticky/firmware.factory.bin` from `mycrossplay`.

## Milestones

- [x] Hello World
- [x] Buttons on screen
- [x] Touch input (GT911)
- [x] A real font engine
- [x] Settings saved to storage
- [x] Wi-Fi connection
- [x] Games home page
- [x] First game: Tic Tac Toe
- [x] Time zone setting and clock chip
- [x] HTTPS downloads
- [x] Second game: Wordle
