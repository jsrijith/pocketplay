# PocketPlay

Games firmware for the [Seeed reTerminal Sticky](https://www.seeedstudio.com/), an e-ink device with a touch screen. It replaces the device's firmware with a simple home page of games, and everything is played by tapping the screen.

**Games included:**

- **Wordle:** the daily puzzle and every past day, with a calendar archive, your record and a chart of your guesses. [How it plays](src/games/wordle/README.md)
- **Tic Tac Toe:** two players on one device.

A game in progress is saved on every move, so it's still there after the device sleeps or the battery runs out.

## What you need

- A Seeed reTerminal Sticky
- A USB-C **data** cable (charge-only cables don't show up on the computer)
- A microSD card formatted as FAT32 or exFAT, to keep your saves and Wordle puzzles
- A computer with Python 3. The steps below use Windows with WSL (Ubuntu), which is the only setup that's been tested

There's no ready-made download yet, so you build the firmware once yourself. The first build downloads its tools and takes several minutes.

## Install

### 1. Build the firmware (in WSL)

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

When it finishes, it prints `Ready to flash` and the image is at `dist/pocketplay-full.bin`. Copy it somewhere Windows can reach, replacing `<you>` with your Windows user name:

```bash
cp dist/pocketplay-full.bin /mnt/c/Users/<you>/Downloads/
```

### 2. Set up flashing (in Windows PowerShell)

WSL can't reach the device over USB, so flashing happens from Windows.

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
2. Open **Settings** and choose your **TIME ZONE**, so Wordle's "today" matches your date.
3. Open **Wordle** and tap **GET PUZZLES**. It joins Wi-Fi if needed, downloads the puzzles, then turns the radio off. After that, playing needs no network until you want new puzzles.

## Using it

| To do this | Do this |
|---|---|
| Open a game or setting | Tap it |
| Sleep | Hold OK for 1 second, or leave it idle for the sleep time |
| Wake | Press OK. A game that was open comes back; otherwise you return to the home page |

**Settings:**

| Row | What it does |
|---|---|
| Wi-Fi | Joins the most recent network, or scans for one |
| Sleep | Idle time before sleeping: 1, 5 or 10 minutes |
| Time zone | Your local time zone |
| Refresh | How many screen updates pass before a half refresh clears ghost images: 5, 10, 15 or 30 (the default), or never |

**Good to know:**

- **Wi-Fi:** it joins 2.4 GHz networks only, not 5 GHz, enterprise or hidden ones. The radio is only on while it's needed, and sleep always turns it off. Up to 4 networks are remembered.
- **Passwords:** Wi-Fi passwords are saved on the SD card scrambled with the chip's MAC address. **This isn't encryption:** anyone with both the card and this code can undo it, so don't lend out a card you've used on a private network.
- **Downloads:** Wordle's downloads don't verify the server's certificate. The device sends no personal data with them, but a network attacker could feed it wrong puzzles.
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

Every firmware build runs the PC tests first and stops if any fail. You can also run them on their own:

```bash
scripts/check.sh    # every test; only "CHECK-VERDICT: green" is a pass
```

To add a game, follow [src/games/README.md](src/games/README.md). A game is one folder and one line in `game_list.inc`.

| Path | Purpose |
|---|---|
| `src/main.cpp` | Boot, input, refreshing the panel, sleep |
| `src/shell/` | Which screen is open, where taps go, and the home page |
| `src/games/` | One folder per game, plus what they share |
| `src/apps/` | One folder per app, when there are some. [The plan](src/apps/README.md) |
| `src/settings/` | The Settings, Wi-Fi and time zone screens, and their saved values |
| `src/ui/` | The buttons, header and keyboard that screens share |
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
4. `setup()` holds the power latches, powers the SD rail, starts input, loads the saves from the card, starts the panel and draws.

Deep sleep turns almost everything off, so waking runs all of this from the start.

## Credits

PocketPlay builds on other people's work, each under its own licence:

- [CrossPlay](https://github.com/ma-r-s/crossplay): the game rules, Wordle's design and the font headers (MIT, copied alongside the code)
- [FreeInk SDK](https://github.com/ma-r-s/freeink-sdk): hardware libraries (MIT)
- [wolfSSL](https://www.wolfssl.com/): HTTPS (GPLv2 or later, or commercial)
- [SdFat](https://github.com/greiman/SdFat): SD card access (MIT)
- Jersey 25: the font (SIL Open Font License)

Wordle is a trademark of The New York Times Company, and this project isn't affiliated with it. The firmware ships no puzzles: the device downloads them when you ask, from [mfilej/wrdl](https://github.com/mfilej/wrdl) and the New York Times.
