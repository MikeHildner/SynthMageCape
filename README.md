# Synth Mage Cape

An LED stage cape for a keyboard player. The lights follow what is played: a
small controller in a belt pouch listens to the keyboard over Bluetooth MIDI
and drives addressable LEDs on the lapels, the collar, the shoulders, and
strings of small lights below the lapels.

![Concept render: the cape lit, from the audience](docs/renders/lights-on-front.png)

The picture is a concept render, not a photograph. A short video of the real
lights on a stand-in cape, following the keyboard: https://youtu.be/IYe8foX2Wks

## Where the project stands (November 2026)

- **Working:** the controller, the firmware, and every light, as a prototype
  on a stand-in cape.
- **Not made yet:** the cape itself. The sheet for the cape maker is
  [docs/designer-handoff.md](docs/designer-handoff.md).
- **Still changing.** This is the record of one build in progress, by a
  software developer who was new to electronics when it started. It is not a
  tested kit. Check the wiring, the power and your own parts before you copy
  anything.

## How it works

- **Keyboard:** a Korg Kronos with a CME WIDI Master in its MIDI OUT, which
  sends what is played as Bluetooth MIDI. Any keyboard that sends Bluetooth
  MIDI should do.
- **Controller:** an ESP32-S3 DevKitC-1 (N16R8) on a small board with a
  74AHCT125 level shifter, powered by a USB power bank. Four data outputs.
- **Lights:**
  - each lapel: two strips of 48 WS2812B LEDs (60 per metre), side by side
  - below each lapel: a string of 50 small WS2812B lights on thin wire,
    chained after the lapel
  - collar: a COB strip (160 per metre), 128 segments
  - each shoulder: two strips of 12 LEDs, side by side
- **Brightness is kept low on purpose.** The cape is for a dark stage, and the
  firmware has a hard ceiling (`MAX_BRIGHTNESS` in `Cape.h`).

What the lights do when you play:

| You do | The cape shows |
|---|---|
| Strike a key | A ripple on the lapels at that key's height. Harder is brighter. The colour is the note name. |
| Hold a key, or use the sustain pedal | A low glow stays on that row |
| Play a low note | The ripple runs off the bottom of the lapel as a dot along the string |
| Play a high note | The ripple runs off the top of the lapel and crosses the shoulders |
| Play several notes | The collar shows one band per sounding note |
| Bend the pitch | The colour of everything lit turns |
| Add vibrato (mod wheel) | The colours swing to and fro |

## What is in the repository

| Folder | What it holds |
|---|---|
| [firmware/cape-midi](firmware/cape-midi) | The firmware (PlatformIO, Arduino framework, FastLED, NimBLE). Start with [firmware/README.md](firmware/README.md). |
| [hardware/shifter-board](hardware/shifter-board) | The level shifter and power distribution board: KiCad files and Gerbers |
| [hardware/perfboard-4](hardware/perfboard-4) | The same circuit laid out for perfboard, which is what the prototype runs on |
| [hardware/enclosure](hardware/enclosure) | A printable box for the controller (OpenSCAD and STL) |
| [docs/electronics-spec.md](docs/electronics-spec.md) | The electronics plan and its test log. Parts of the plan are older than the build; where the two disagree, the firmware and the handoff are newer. |
| [docs/shopping-list.md](docs/shopping-list.md) | Parts and tools, with the reasons for each choice |
| [docs/bench-session-1.md](docs/bench-session-1.md), [docs/perfboard-4-build.md](docs/perfboard-4-build.md) | Step-by-step build guides for the controller board |
| [docs/designer-handoff.md](docs/designer-handoff.md) | What the garment needs from the electronics, written for whoever sews the cape (also as a PDF) |
| [docs/render-prompts.md](docs/render-prompts.md), [docs/renders](docs/renders) | The concept renders and the prompts that made them |
| [apps/midi-monitor-android](apps/midi-monitor-android) | A small Android app that shows what a Bluetooth MIDI device sends. Useful for finding out what your own keyboard sends. |
| [wled](wled) | Settings from the first phase, when the controller ran WLED |
| [tools](tools) | Scripts: build and update the firmware, generate the board drawings and the handoff PDF |

## Building the firmware

1. Install [PlatformIO Core](https://platformio.org/install/cli).
2. In `firmware/cape-midi/include`, copy `secrets.example.h` to `secrets.h`
   and fill in your Wi-Fi name and password. The cape uses Wi-Fi at home only,
   for updates and a status page; away from home it runs on Bluetooth alone.
   `secrets.h` is ignored by git.
3. From `firmware/cape-midi`, run `pio run -t upload` with the board on USB.

`tools\pio.cmd` and `tools\ota.cmd` are Windows helpers for my own setup (one
works around an antivirus certificate problem, the other sends an update over
Wi-Fi to my cape's address). [firmware/README.md](firmware/README.md) explains
both.

## Licence

MIT. See [LICENSE](LICENSE).
