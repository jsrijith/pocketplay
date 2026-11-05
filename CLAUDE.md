# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

BoardByte is a games-only firmware for the Seeed reTerminal Sticky (ESP32-S3), built up one milestone at a time so its owner can learn how ESP32 firmware works. Keep each step small, and explain the hardware reasoning. The milestone list is at the end of `README.md`.

## Commands

Run these from WSL. PlatformIO isn't on PATH.

```bash
./scripts/package.sh                      # build and write dist/boardbyte-full.bin
~/.platformio/penv/bin/pio run -e sticky  # build only
```

There are no tests or linters yet. A build only proves the code compiles, so behaviour has to be confirmed on the device.

**Flashing is done by the user, from Windows.** WSL has no USB access to the device. Give the user commands to run in PowerShell, after they copy the image to `C:\Users\Srijith\Downloads`:

```powershell
python -m esptool --chip esp32s3 --port COM8 --baud 460800 write-flash 0x0 boardbyte-full.bin
python -m serial.tools.miniterm COM8 115200
```

Use `python`, because this machine has no `py` launcher. Always flash the merged image at `0x0`, never the app alone: the merged image includes `boot_app0.bin`, which selects app0 if an earlier firmware last booted from app1.

`gh` exists only as `gh.exe` on the Windows side. WSL has no stored GitHub credentials, so a push needs `-c credential.helper= -c 'credential.helper=!gh.exe auth git-credential'`.

## Architecture

`src/main.cpp` is an Arduino `setup()`/`loop()` program, with drawing in `src/graphics.cpp` and text in `src/text.cpp`. It runs on top of four `freeink-sdk` libraries (BoardConfig, EInkDisplay, InputManager, PowerManager), which are symlinked through `lib_deps` in `platformio.ini`. `-DFREEINK_DEVICE_STICKY=1` makes the SDK's `BoardConfig::ACTIVE` resolve to the Sticky's pins and panel settings, so take pins from `ACTIVE` rather than hardcoding them. To use another SDK library (touch, input, sensors), add it to `lib_deps`. Its API is in `freeink-sdk/libs/*/*/include/`.

`freeink-sdk/` is a submodule pinned to the commit CrossPlay builds against. Don't edit it.

**Drawing:** code writes into the framebuffer from `display.getFrameBuffer()`, then `displayBufferAsync()` sends it to the panel. The framebuffer is 48,000 bytes in native 800x480 landscape: 1 bit per pixel, MSB first, 1 = white. There is no graphics library; `src/graphics.cpp` has the pixel and rectangle functions.

**Text:** `src/text.cpp` draws Jersey 25 from the headers in `src/fonts/`, which are copied unchanged from CrossPlay. Read `src/fonts/README.md` before replacing one, because regenerating them with newer tools moves text on every screen. Keep fonts 1-bit: this panel draws pure black and white, and a pixel font has no grey edges to lose. Take line height and capital height from `text.h`, never as numbers copied from a font, and centre labels with `drawTextInBox()`, because centring on the line box makes capitals hang low. Text is ASCII, and any other byte draws as an outlined box. The font headers define `static` data, so include them only from `text.cpp`, or each file that includes them gets its own copy.

**Orientation:** the firmware draws in portrait, 480 wide by 800 tall, held with the USB edge on the right. `setPixel()` is the only drawing code that knows the panel's native landscape layout, and `touchToScreenX()` and `touchToScreenY()` do the same for touch.

**Refreshing:** `displayBufferAsync()` returns once the frame is sent, so `loop()` keeps reading buttons while the panel updates. It allocates one 48 KB shadow frame on first use and keeps it. Once async refreshes have started, don't call a blocking `displayBuffer(FAST_REFRESH)`: in single-buffer mode it compares against the panel's stored previous frame, which async refreshes leave out of date. To wait for a frame, call `displayBufferAsync()` and then `waitRefreshComplete()`.

**Input:** buttons and touch both go through the SDK's `InputManager`. Call `update()` once per loop, then read its events; they only last until the next `update()`. On this board it doesn't debounce buttons, so the 15 ms loop is the debounce: polling much faster can count one press twice. OK is read as `BTN_POWER`, because `setSharedConfirmPowerShortPressEmitsPower(true)` makes it follow the button straight away instead of reporting `BTN_CONFIRM` on release. Touch positions arrive as 0..1 in the panel's landscape frame.

## Hardware rules

These cause silent failures, and the build won't catch them:

- **Boot order in `setup()` matters.** `BoardConfig::holdPowerRails()` runs first, then `BoardConfig::releaseSdRail()`, then `display.begin()`. The SD card shares the panel's SPI bus, and an unpowered card clamps the bus so the panel never hears a command.
- **`gpio_hold_en` survives resets and reflashing.** A held pin silently ignores writes, so call `gpio_hold_dis` before driving any pin that a sleep path, including another firmware's, may have held.
- **Sleep order in `goToSleep()` matters.** `display.deepSleep()` runs first while the panel still has power, then the latches on GPIO45/46 are held HIGH, then the rails are cut, then the chip sleeps.
- **Waking is a full chip reset.** Every wake runs `setup()` again. Only the OK button (GPIO4) can wake the device, since touch power is cut during sleep. OK must be released once after boot before a hold counts as sleep.
- **USB-C reaches UART0 through a CH343 chip, not native USB.** That's why `ARDUINO_USB_CDC_ON_BOOT=0` is set, which differs from CrossPlay. Opening the serial port resets the device.
- **GPIO0 is a strapping pin** and also the sensor bus SCL. Don't leave a pull on it that disturbs boot mode.
- **Keep the device recoverable.** Never write eFuses or enable secure boot or flash encryption, since those can permanently lock the chip.

## Reference

`/home/srijith/mycrossplay` is a working full firmware for this device, built on the same SDK. Read it to see how something is done on the Sticky, but don't modify it or copy its structure wholesale. Its `CLAUDE.md` describes that project and doesn't apply here.
