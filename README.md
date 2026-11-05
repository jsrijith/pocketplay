# BoardByte

A games firmware for the Seeed reTerminal Sticky, built up one step at a time.
Right now it shows the buttons and touch input on screen, with sample text at each font size. A box turns black while its button is held, and counts its presses. Tapping the screen moves a cross to your finger and prints its position, and swipes show their direction.

- **Hold OK for 1 second:** sleep. The screen shows `ASLEEP`.
- **No button activity for 10 minutes:** sleep.
- **Press OK while asleep:** the chip restarts and redraws the screen, with the counts back at 0.

## The hardware

| Part | Details |
|---|---|
| Chip | ESP32-S3: dual-core, 16MB flash, 8MB PSRAM |
| Screen | 3.97" 800x480 black and white e-ink, SSD1677 controller, SPI |
| USB-C | Goes through a CH343 USB-serial chip to UART0, not the S3's native USB |
| Buttons | OK/Power on GPIO4, Up on GPIO5, Down on GPIO6, all active low |
| Touch | GT911 over I2C: SDA GPIO3, SCL GPIO2, INT GPIO21, RST GPIO41, power enable GPIO42 |

## Files

| File | Purpose |
|---|---|
| `platformio.ini` | Chip, flash layout, compiler flags, libraries |
| `partitions.csv` | How the 16MB flash is divided |
| `src/main.cpp` | The firmware: input, the screen layout, sleep |
| `src/graphics.h`, `graphics.cpp` | Pixels, rectangles and the portrait mapping |
| `src/text.h`, `text.cpp` | Drawing, measuring, aligning and wrapping text |
| `src/fonts/` | Jersey 25 at six sizes, copied from CrossPlay, with its licences |
| `freeink-sdk/` | Screen and power libraries, as a git submodule |
| `scripts/package.sh` | Builds and produces `dist/boardbyte-full.bin` |

After cloning, run `git submodule update --init freeink-sdk`.

## How it boots

1. The ROM bootloader runs. It's built into the chip and can't be overwritten.
2. It loads the second-stage bootloader from flash at `0x0`.
3. That reads the partition table at `0x8000`, then `otadata` at `0xe000` to pick an app slot.
4. The app starts from `0x10000`. Arduino starts FreeRTOS, then calls `setup()` once and `loop()` repeatedly.
5. `setup()` holds the power latches, powers the SD rail, starts the panel and draws.

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

Flash over this one the same way, as often as you like. The SD card isn't touched.

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
- [ ] Settings saved to storage
- [ ] Wi-Fi connection
- [ ] Games home page
- [ ] First game
