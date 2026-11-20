## What's new

{WHATS_NEW}

## Install

Read the [Disclaimer](https://github.com/jsrijith/pocketplay#disclaimer) first, and back up your current firmware as the [README](https://github.com/jsrijith/pocketplay#3-back-up-your-current-firmware) describes.

Download `pocketplay-full.bin` below, then flash it at offset `0x0` (replace `COM8` with your device's port):

```
python -m esptool --chip esp32s3 --port COM8 --baud 460800 write-flash 0x0 pocketplay-full.bin
```

Always flash this whole image at `0x0`, never only part of it. Your saves are kept.

Coming from another firmware? The [README](https://github.com/jsrijith/pocketplay#4-flash-pocketplay) has an optional one-time step that stops a harmless `Core dump data check failed` message at boot.

**SHA-256:** `{SHA256}`

The boot screen should show `{BOOT_LINE}`.

## Licence

PocketPlay's own code is MIT. The image links wolfSSL, so the image as a whole is under the GPL (version 2 or later); this tag is its source. See [Credits](https://github.com/jsrijith/pocketplay#credits) for the projects it builds on.
