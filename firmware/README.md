# Cape firmware (phase 2, MIDI)

`cape-midi/` is the custom firmware that listens to the Korg Kronos over
Bluetooth MIDI (through the CME WIDI Master) and drives the cape's LEDs. It is
a separate program from WLED: one ESP32 runs one or the other. It is developed
on a spare ESP32-S3 so the cape's own controller keeps WLED.

## Building

PlatformIO Core 6.2.0 is installed in Python 3.12 (`pip install platformio`).
From the repository root, in PowerShell or a command prompt:

    tools\pio run               # build
    tools\pio run -t upload     # build and flash the board on USB
    tools\pio device monitor    # show what the board prints (Ctrl+C to stop)

Plug the board straight into the computer by its UART socket. It shows up as a
CH343 serial port.

Versions are pinned in `cape-midi/platformio.ini`: platform espressif32 8.0.0,
which brings Arduino-ESP32 core 3.3.x. PlatformIO keeps the compiler and
framework in `%USERPROFILE%\.platformio` (about 5.6 GB after the first build,
1.3 GB of it a download cache). Build output goes to `cape-midi/.pio/`, which
git ignores.

## The Norton certificate

Norton 360's web shield inspects secure traffic by re-signing it with its own
root certificate ("Norton Web/Mail Shield Root"). Windows trusts that root, so
browsers, curl and pip work. PlatformIO uses the `certifi` trust list instead,
which does not include it, so every download failed with
`CERTIFICATE_VERIFY_FAILED`.

The fix used here, set up on 16 Nov 2026: a file
`%USERPROFILE%\.platformio\ca-bundle-with-norton.pem` holding certifi's list
plus Norton's root copied from the Windows trusted root store. `tools/pio.cmd`
sets `REQUESTS_CA_BUNDLE` to that file for the one command it runs. Nothing
system-wide was changed and certificate checking stays on.

Running `python -m platformio` directly, or the PlatformIO extension in
VS Code, will hit the same error unless `REQUESTS_CA_BUNDLE` is set the same
way. Only downloads need it; a build with everything already installed does
not go online.

## Rules carried over from the bench

- Brightness: `MAX_BRIGHTNESS` in the code is a hard ceiling. The cape is for a
  dark stage and must not overwhelm the other players or the audience.
- Same GPIO numbers as the WLED setup (data on 4 and 5, buttons on 9, 10, 11),
  so a lead moves to the same pin on the other board.
