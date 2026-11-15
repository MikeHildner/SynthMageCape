# Synth Mage Cape: Revised Electronics and Firmware Spec

Companion to `Synth_Mage_Cape_Designer_Brief.pdf`. This document replaces page 4 (Lighting + Control Architecture) and section B of page 5 (Electronics BOM). The garment-side decisions in the PDF (open-front cape, removable electronics, waist pouch, connectors at shoulders and waist, fabric channels with diffusion mesh) are unchanged and this spec is built around them.

Revision date: 20 October 2026.

## 1. What changed from the brief, and why

| Brief said | This spec says | Reason |
|---|---|---|
| 149 pixels, 30 px/m strip | ~470 pixels, 60 px/m on lapels, COB outlining the whole collar | 30 px/m reads as a dotted line. The renders show panels and continuous edge glow. |
| One data line through a level shifter to "5-7 zones" | Four independent data outputs, one per quadrant (star) | A dead pixel on a single chain blacks out everything downstream. |
| Chest mic (phase 1), drum MIDI (phase 2) | Chest mic (phase 1), **mic plus BLE MIDI from the Korg Kronos** layered together (phase 2), lighting desk (phase 3) | You are the keyboard player. Your MIDI is already on stage. The mic keeps tracking the band's kick underneath it. Drum MIDI is dropped. |
| "20,000 mAh USB bank, 5V/3A verified" | Bank with an always-on / low-current mode, wired from a port that really delivers 5V at 2.4A or more | Most banks shut off under light load and many only do 3A on USB-C with negotiation. |
| WLED or custom | WLED first, custom FastLED firmware only for MIDI | WLED gives you 90% of the show for free. |

## 2. Zone-by-zone pixel plan

All pixels are 5V addressable (WS2812B protocol). Lengths are planning figures; the designer confirms at the first fitting. Worst case is every pixel at full white, 60 mA each, and is only used to size wiring and fuses. It is never a running condition.

| Zone | Where | Type | Length | Pixels | Worst case |
|---|---|---|---|---|---|
| A | Collar: both front edges plus the full top rim | WS2812B COB strip, 5V, ~160 px/m, 8-10 mm wide | 2 × 0.3 m + ~0.5 m rim = ~1.1 m | ~176 | ~2.6 A (COB is rated per metre, ~2.4 A/m) |
| B | Lapels, two columns each side | WS2812B strip, 5V, 60 px/m, IP65 silicone sleeve | 4 × 0.8 m | 192 | 11.5 A |
| C | Shoulder accents | WS2812B strip, 60 px/m, IP65 | 2 × 0.2 m | 24 | 1.4 A |
| D | Lower front edges, waist to 15 cm above hem | 12 mm WS2811 bullet pixel string, 5V, 10 cm spacing (the stock spacing) | 2 × 8 px | 16 | 1.0 A |
| E | Outer side seams, 15 cm above hem up toward the waist | 12 mm WS2811 bullet pixel string, 5V, 10 cm spacing | 2 × 12 px | 24 | 1.4 A |
| | **Total** | | | **~430** | **~19 A / 95 W** |

Notes on each zone:

- **A, Collar.** COB strip has the LEDs under a continuous phosphor layer, so you get an unbroken line of light with no visible dots. The line runs up each straight front edge and continues along the full top rim of the collar around the back of the neck, so the collar reads as one outlined shape from the front (adopted from the 20 Oct render). This is feasible because a strip laid flat against the collar face bends the easy way as the rim curves around the neck; only the corner where the front edge meets the rim needs a cut and a short solder jumper. Split the loop at centre back so each half (front edge plus half the rim, ~0.55 m) feeds from its own side's front output. Verify the "pixels per metre" figure on the listing, not just "LEDs per metre": some COB strips group several LEDs per controller IC. Fallback if COB is hard to source: 60 px/m strip set back 15-20 mm behind the diffuser, front edges only.
- **B, Lapels.** Two parallel columns per lapel, 10-15 mm apart under one diffuser, gives a panel feel and lets you run two-column equalizer effects. If budget or current is tight, drop to one column per lapel (96 pixels total, worst case 5.8 A) and the rest of this spec still holds.
- **C, Shoulders.** Short strip segments on the stiff epaulet pieces. These are the easiest to leave out if the designer wants cleaner shoulders.
- **D and E, Drape.** Bullet-style string pixels are the only type that survives fabric that folds and swings. They are individually sealed, on flexible wire, and replaceable one at a time. Push each bullet through a small grommet or sew into a mesh sleeve. Zone E runs along the outer side seam of each side panel rather than scattered across the panel (adopted from the 20 Oct render): a string along a seam is easier to mount and reads as a clean line of dots at the 3/4 angle. **Hem rule for both zones: the lowest bullet sits at least 15 cm above the hem.** Bullets at floor level get stepped on, kicked, and dragged.

Realistic current: with the WLED limiter set to 2.5 A, the controller scales brightness so the total never exceeds that. Across ~430 pixels that is an average budget of about 6 mA per pixel, which means roughly 10% brightness if every pixel is lit white at once, or 30-50% brightness for typical colored animations where a third of the pixels are lit. In a dark venue, WS2812B at 30% is bright. If you later want more headroom, add a second bank for the lapels (section 4.5).

Idle current matters too: each WS2812B draws about 0.6-1 mA when dark, so ~430 dark pixels plus the ESP32 pull roughly 0.4-0.6 A. That is above the shutoff threshold of most power banks, which largely solves the auto-off problem on its own. You still test it (section 7).

## 3. Topology and wiring

### 3.1 Star layout

Four data outputs from the controller, each feeding one quadrant. Power is delivered to each quadrant separately from a fused distribution block, so every quadrant gets fresh 5V and is not relying on the quadrant before it.

```
                                 +-----------------------+
   USB bank (waist pouch) ---->  | SWITCH -> 3A FUSE     |
                                 | 1000 uF cap           |
                                 | 5V distribution block |
                                 +---+---+---+---+---+---+
                                     |   |   |   |   |
                     5V + GND to ----+   |   |   |   +---- 5V + GND to controller (VIN pin)
                     each quadrant       |   |   |
                                         |   |   |
   +----------------+   4 data lines    +-------------+   4 data lines (5V logic)
   | ESP32-S3       | ----------------> | 74AHCT125   | ------------------------------+
   | WLED / custom  |  (3.3V logic)     | level shift |                               |
   | INMP441 mic    |                   | + 220R each |                               |
   | 3 buttons      |                   +-------------+                               |
   +----------------+                                                                 |
                                                                                      |
   OUT1: LEFT FRONT   = collar left half (front edge + half rim) -> left lapel col 1 -> col 2 -> left shoulder    (~196 px)
   OUT2: RIGHT FRONT  = collar right half (front edge + half rim) -> right lapel col 1 -> col 2 -> right shoulder (~196 px)
   OUT3: LEFT DRAPE   = left lower front edge string -> left outer side seam string                                (~20 px)
   OUT4: RIGHT DRAPE  = right lower front edge string -> right outer side seam string                              (~20 px)
```

Each output is a 3-pin cable (5V, GND, DATA) from the pouch up to a connector at the shoulder (front quadrants) or waist (drape quadrants), matching the connector points already in the brief. Inside a quadrant the segments chain with short jumpers.

Why four and not six: the ESP32-S3 has four hardware-timed (RMT) transmit channels, which is the guaranteed-safe number for WLED. WLED can drive more outputs on the S3 via its parallel I2S mode; if that works on your board, split the collar halves onto their own outputs for a six-way star. Four is the baseline and the quad 74AHCT125 matches it exactly.

Why chain within a quadrant: it keeps the cable count down to four. A failure still only takes out one quarter of the cape, and the chain order puts the most robust, best-supported segments (collar on a stiff backing) first.

### 3.2 Power injection

Each quadrant gets 5V and GND at its connector. For the front quadrants, also run a second 5V/GND pair to the far end of the lapel columns (the bottom of the lapel), because 1.6 m of 60 px/m strip in series will show voltage droop as a yellowish tint at the far end on bright white. The drape quadrants are short enough not to need it.

Wire gauge for 5V runs: 20 AWG silicone. At 20 AWG a 1.5 m run out and back is about 0.1 ohm, so 2 A drops 0.2 V, which is acceptable. Do not use the thin wires that come pre-attached to strips for anything longer than 20 cm.

Data wire: 24 or 26 AWG, twisted or run alongside the GND wire. Keep each data run under 2 m. Put a 220 ohm resistor in series at the level shifter output on each line.

### 3.3 Protection

| Item | Spec | Where |
|---|---|---|
| Main switch | Rocker or toggle, 5 A or more, reachable without help | Pouch exterior or belt, per acceptance checklist |
| Main fuse | 3 A fast-blow, inline holder, replaceable | Immediately after the switch, before anything else |
| Bulk capacitor | 1000 uF, 10 V or 16 V electrolytic | Across 5V/GND at the distribution block |
| Per-quadrant fuse (optional) | 2 A polyfuse (resettable) | At each distribution output |
| Data line resistor | 220 ohm | Between level shifter and connector, each line |

The bank has its own short-circuit protection. The fuse is there so that a chafed wire in the garment blows a 3 A fuse rather than relying on the bank's limiter and heating the wire first.

### 3.4 Connectors

| Use | Connector | Why |
|---|---|---|
| Quadrant feed (5V, GND, DATA) | JST-SM 3-pin, 22 AWG leads | Standard on LED strip, locking, rated 3 A per pin |
| Lapel bottom power injection | JST-SM 2-pin or XT30 | Keyed, cannot plug into a data port |
| Bank to pouch electronics | USB-A to bare-wire cable, 20 AWG, 50 cm | See section 4 |
| Strip-to-strip within a quadrant | Soldered with heat shrink, or JST-SM 3-pin where the designer needs a seam break | Fewer connectors means fewer failure points; add one only where the garment needs to come apart |

Mark every connector pair with matching colored heat shrink so reassembly after cleaning is foolproof.

### 3.5 Mounting rules for the designer

- Strip (A, B, C) goes only on supported, non-folding areas: stiffened collar, lapel facing, shoulder piece. Keep the strip's silicone sleeve on and sew a fabric channel around it. Never put rigid strip where the fabric creases.
- The LED strip's weak points are the solder joints at each end. Anchor the strip 2-3 cm from each end so the joint itself carries no load.
- Bullet strings (D, E) can go anywhere the fabric moves. Space the grommets to match the string's native spacing, do not stretch the string.
- Leave a finger-width of slack at every connector, and a fabric loop or tab the cable can be tied to, so pulling on the cape never pulls on a solder joint.
- The controller box and bank live in the waist pouch. Only cables leave the pouch.

## 4. Power

### 4.1 Runtime math

20,000 mAh bank at 3.7 V nominal is 74 Wh. Assume 85% conversion to 5 V: about 63 Wh, or 12.6 Ah at 5 V.

| Load | Runtime |
|---|---|
| 2.5 A, the software cap, continuously | ~5 h |
| 1.2 A, realistic show average | ~10 h |
| Two 45-minute sets plus changeover | 1.5-2 h needed |

A 10,000 mAh bank gives half of that and still clears two sets comfortably while weighing about 200 g less. Buy the 20,000 if you want to stop thinking about charging between gigs; buy the 10,000 if the designer flags weight at the waist.

### 4.2 Bank selection criteria

1. **An always-on, trickle-charge, or low-current mode.** This is the make-or-break feature. Several Anker PowerCore models have a trickle mode entered by holding the button; INIU banks sell a "low current mode"; Voltaic Systems banks are built with always-on as a feature for exactly this kind of project. The product page or manual will say. If it does not say, assume it shuts off.
2. **A USB-A port rated at 5V / 2.4 A or higher**, or a USB-C port rated 5V / 3A. You only need 2.5 A.
3. **Pass-through charging is not required** and often disables output while charging, so do not plan on charging during a show.
4. **No "smart" features that cut output when current changes suddenly.** Some PD banks renegotiate on load steps. A plain USB-A port avoids all of that.

### 4.3 USB-A versus USB-C

Use the **USB-A port** with a USB-A to bare-wire cable (20 AWG, 2 conductors). A USB-A port simply outputs 5 V. This is the lowest-risk option and 2.4 A is enough.

If you use USB-C: you must use a breakout board that has 5.1 k ohm pulldown resistors on both CC pins (for example the Adafruit USB-C breakout, or any listing that says "with CC resistors"). Without them, a USB-C bank outputs nothing. Do not cut open a C-to-C cable; the plug end has no CC resistors.

### 4.4 Keep-alive load

Probably unnecessary at ~400 pixels (section 2, idle current), but if a 10-minute idle test shows the bank dropping out, add one of:

- A 47 ohm, 2 W resistor across 5V/GND at the distribution block. Draws ~106 mA, wastes 0.5 W, costs nothing in complexity.
- A WLED "idle" preset that keeps a few pixels dimly lit instead of fully black. Free, and looks intentional on stage.

### 4.5 Second bank option

If you want the lapels brighter than the 2.5 A budget allows, give the two front quadrants their own bank and leave the drape quadrants and controller on the first. Tie the GND of both banks together at the distribution block (USB banks are isolated, so this is safe) and set two WLED limiter values by using two controllers, or simply accept that the limiter is global and set it to the combined budget. Not needed for the first build.

## 5. Controller and sensors

| Part | Detail |
|---|---|
| ESP32-S3 dev board | ESP32-S3-DevKitC-1 or equivalent with 8 MB flash and PSRAM (N8R2 or N16R8). The PSRAM helps WLED audio-reactive and gives headroom for custom firmware. |
| Level shifter | SN74AHCT125N (DIP-14) in a socket on a bit of perfboard, or a pre-made breakout. Four channels, one per output. Power it from 5 V. Tie the four output-enable pins to GND. |
| Microphone | INMP441 I2S MEMS mic, mounted on the outward face of the controller box or pouch at the hip, port exposed through a small open-mesh patch, foam windscreen over the port. Not under velvet, not inside the pouch. Three I2S pins plus 3.3 V and GND, lead under 15 cm. |
| Buttons | Three 12 mm momentary panel-mount buttons on the pouch flap: on/off, next preset, brightness. WLED supports these natively. |
| Enclosure | Printed tray with a screw-down lid, 97 x 91 x 34 mm outside plus screw heads, model in `hardware/enclosure/controller-box.scad`. Boards side by side on pegs and corner blocks, clamped by posts under the lid; four M3 x 10 screws cut their own thread in the corner pillars. ESP32 antenna at the plain end wall; USB cable and the four strip leads leave through labelled slots in the other end. No USB opening: lift the ESP32 out to reflash by wire. Mic port and button wiring still to be added. Lives in the pouch. First print (PLA) passed 12 Nov 2026; the worn version is to be PETG. Replaces the bought 90 x 70 x 28 mm ABS box, which was too small. |

GPIO: on the S3 avoid the strapping pins (0, 3, 45, 46), the PSRAM pins (typically 26-37 on boards with octal PSRAM), and the USB pins (19, 20). Good choices for the four LED outputs are 4, 5, 6 and 7. Set the I2S mic pins and button pins in WLED's settings pages; any free GPIO works.

Power the ESP32 from the fused 5 V bus into its 5V/VIN pin. LED current does not pass through the board, which keeps the brief's guardrail.

## 6. Firmware roadmap

### Phase 1: WLED (target: first fitting with lights)

Flash the ESP32-S3 audio-reactive build of WLED 0.15 or later from the official web installer. Configure:

1. **LED settings:** four outputs, type WS2812B (GRB), pixel counts per quadrant from section 2. Set the **maximum current** (the limiter) to 2500 mA and enter the per-LED current as 55 mA. Leave the limiter on permanently.
2. **Segments:** create one segment per visual zone (collar, left lapel, right lapel, shoulders, left drape, right drape) so effects can differ per zone even though they share a quadrant output.
3. **Audio reactive usermod:** set the INMP441 pins, choose the I2S "generic" mic type, then set gain and squelch during a loud-ish rehearsal. The chest mic will mostly track the overall mix and bass.
4. **Presets:** Idle (slow dim breathing, keeps the bank awake), Song A (GEQ on the lapels, Ripple Peak on the collar, Pixels or Twinkle on the strings), Song B (a non-audio animation for quiet moments), Blackout. Put them in a playlist and map the buttons.
5. **Soak test:** run Song A for two hours on the bank and watch the current on a USB inline meter.

What this gets you: a complete, controllable, music-reactive show with a phone app, before a line of code is written.

### Phase 2: Mic plus Kronos MIDI, layered (the target show)

Two inputs, two layers, custom firmware.

**Background layer, from the mic.** The INMP441 tracks the band. Low-band energy drives a kick pulse on the collar and strings; overall level sets a breathing floor brightness; a few frequency bands feed a spectrum on the lapels when you are not playing. A single chest mic hears the whole mix, so kick is reliable and snare or toms are not. The brief's per-drum color mapping is dropped; it would need a feed from the drummer's module.

**Foreground layer, from the Korg Kronos.** Note-on fires a ripple up the lapel at a height set by pitch, velocity sets its brightness, sustain pedal holds the glow, pitch bend shifts hue, chord density (notes held at once) triggers sparkle on the strings. If the Kronos is set to transmit MIDI clock (Global settings), tempo-locked effects follow the arpeggiator and sequencer.

**Mode button:** band only, keys only, both. "Both" is the default: your notes ride on the band's pulse, and when you stop playing the band keeps the cape alive.

**Getting Kronos MIDI into the cape: BLE MIDI via the 5-pin DIN MIDI OUT.** The Kronos has no Bluetooth of its own, and its USB-B port is a device port meant for a computer. A CME WIDI Master plugs directly into the Kronos MIDI OUT and is powered by that jack, no battery. The ESP32-S3 connects to the WIDI over BLE MIDI as the initiating side, the same way a tablet would. Latency is 10-20 ms, invisible on lights. One Kronos only; a second keyboard would need a MIDI merge box ahead of the WIDI.

The wired alternative, a MIDI cable from the Kronos to a DIN jack on the pouch through a 6N138 optocoupler into a UART at 31250 baud, costs $5 and is bulletproof, but it runs a cable from the keyboard to your belt across the stage. Keep it as the fallback if BLE pairing proves flaky in a venue.

**Firmware:** a FastLED sketch on the Arduino framework. The Arduino BLE-MIDI library on the NimBLE stack handles the Kronos link in client mode. The audio layer is re-implemented here because WLED's engine does not come along: an FFT library (arduinoFFT) over the I2S samples into 4 to 8 bands, a simple onset detector on the low band for kick, and automatic gain so the venue's volume does not matter. That is a few hundred lines, not WLED's full engine, and it is enough for the background layer. Structure the effects as per-zone buffers into which both layers push "sparks" that decay each frame, so the show degrades gracefully when either input goes quiet. Keep the WLED power-limiter idea: sum the frame's requested brightness and scale down when it exceeds the budget. Keep the WLED image around and flash back to it as a fallback; both live on the same board.

If you would rather stay inside WLED, the alternative is a usermod that receives BLE MIDI and drives presets or segment brightness through WLED's internal API. Fitting NimBLE alongside WiFi and the audio usermod in WLED's memory budget is harder than writing the standalone sketch, so the FastLED sketch is the recommended path.

### Phase 3: Lighting desk or laptop control (optional)

WLED natively accepts Art-Net, E1.31 (sACN), and DDP over WiFi. A lighting tech, or a laptop running LedFx on the venue's line feed, can drive the cape like any other fixture. Requirements: the cape joins a dedicated 2.4 GHz access point you bring, not venue WiFi. Treat it as a bonus for shows with a lighting person, not the baseline, because stage WiFi is unreliable.

### Asides, not on the plan

- **Line-in instead of the mic.** WLED's audio-reactive build also supports I2S line-in boards (ES7243, PCM1808). A cable from a Kronos individual out would give the phase 1 show a clean signal of your own playing, but it tethers you, and phase 2 gets the same result over the air.
- **USB host.** The ESP32-S3's native USB port can act as a USB MIDI host to the Kronos USB-B port using the ESP-IDF USB host MIDI example. More firmware work than DIN and still a cable.

## 7. Bring-up and test sequence

1. **Bench, bench supply:** flash WLED, wire one output to one lapel column on a 5 V bench supply with a current meter. Confirm colors, order (GRB), and the limiter.
2. **All four quadrants on the bench supply:** confirm each output independently. Measure worst case by temporarily setting the limiter high and white at 100% for ten seconds, note the current, then set the limiter back to 2500 mA.
3. **Bank test:** move to the USB bank through the switch and fuse. Run the Idle preset for 15 minutes and confirm the bank stays on. Run Song A for two hours and log the inline meter every 30 minutes.
4. **Flex test:** with the modules in their sleeves but before the designer sews channels, fold and swing each segment a hundred times. Anything that flickers gets re-soldered and strain-relieved now.
5. **Hand modules to the designer** with dummies: a 30 cm dead piece of strip and a short dead string so channels and grommets are sized against real parts. Give them the finished modules only after step 4.
6. **After sewing:** full show on the bank, at the Kronos, through the acceptance checklist on page 6 of the brief. Add: touch every connector and the bank after the two-hour run; nothing should be more than warm.

### Test log

- **25 Oct 2026, bank 1 of 2 (Anker 313 PowerCore 10K, A1229), bare ESP32-S3 DevKitC-1 as load.** Trickle mode on: held the board for 15 minutes. Trickle mode off: still held the board for 10 minutes without shutting off, so the bank's auto-off threshold is below the bare board's draw. Trickle mode is insurance only for this bank; the lit cape's idle draw is several times higher. Bank 2: same result. Bare board draws 51 mA at 5.06 V on the USB meter.
- **25 Oct 2026, WLED flashed.** WLED 16.0.1, build ESP32-S3 16MB opi, via install.wled.me (Advanced, Plain, 16MB, Standard memory, Default flash mode; the installer picks the S3 OPI file itself, and audio-reactive is built into the standard build since 0.15). Board serial chip is a CH343 on COM5, driver found by Windows. Board is on home WiFi; wled.local does not resolve from Windows, use the IP and reserve it in the router. WLED Android app sees it. Settings page is Config > LED & Hardware in this version.
- **25 Oct 2026, first light.** Onboard RGB LED driven from WLED with output 1 set to GPIO 48, length 1, GRB: all colors and brightness follow the app. The LED settings page reports "Hardware channels used: RMT 1/4, I2S 0/8", so this build can drive up to 12 outputs on the S3; the six-way star (collar halves on their own outputs) is available without any hardware change. Keep GPIO 48 as a one-pixel status output; the four cape outputs go on GPIO 4, 5, 6, 7 as planned.
- **26 Oct 2026, shifter board started (second board; the first is a practice board).** DIP-14 socket soldered on the ELEGOO 5 x 7 cm perfboard, letters J to P, pins in columns 09 and 12, notch toward P, pin 1 at P12. Continuity checks pass: all 14 pin-to-joint pairs beep, no beep between any neighbouring pins or across the socket. Rails wait for the Jameco bus wire. Layout drawings: docs/diagrams/perfboard-grid.png, shifter-board-step1-rails.png, shifter-board-full.png.
- **4 Nov 2026, third board: socket, rails, capacitor, six pin wires, outputs A and B resistors done and tested.** Sequence used: socket, then rails with G01/G03 left open, then capacitor legs through those holes soldered on the back, then pin wires laid onto the existing cones. Resistor legs bent onto the pin cone instead of a solder bridge. Fault found: output B leg at K12 bridged to the pin 7 ground cone at J12 one hole above; cleared with wick. Lesson: once ground is a net, probing other ground points cannot locate a short; probe the non-ground neighbours and look. A single reading that contradicts the others is a bad probe contact until repeated. Stickvise now holds the board.
- **6 Nov 2026, third board abandoned at the leads stage.** Resistors C and D, pigtail A (green on N17, red on S03, white on K01), GPIO 4 lead (yellow, pin 2 cone at O12), GPIO 5 lead (brown, pin 5 cone at L12) and the ESP 5 V lead (red, I03) all went on and tested. The ESP GND lead on I01 shorted the rails. Two causes: the leads had been stripped along their length instead of 3 mm at the tip, so bare wire lay across the back of the board, and the 5 V and GND rails are two holes apart, so any bare lead near one can touch the other. Also wicked an O12 to P12 bridge from the GPIO 4 lead. Decision: start a fourth board from a new layout instead of repairing. Rules for board four: strip 3 mm at the joint only, sleeve any longer bare run, rails on opposite edges of the board, no wire crosses a pin, every lead through its own hole from the front with the rail wire soldered last.
- **7 Nov 2026, schematic in KiCad 10.** `hardware/shifter-board/shifter-board.kicad_sch`, generated by `tools/make_shifter_schematic.py`, rendered to `docs/diagrams/shifter-board-schematic.pdf`. Electrical rules check: zero errors. Netlist checked by hand against the pinout table: all four buffers, enables and pin 7 on GND, pin 14 on 5 V, 220 R on every output, four JST-SM outputs (pin 1 red 5 V, pin 2 green data, pin 3 white GND), C1 across the rails, USB in, six leads to the ESP32-S3. This is now the single reference for the board; the perfboard drawings are derived from it.
- **7 Nov 2026, PCB laid out in KiCad 10.** `hardware/shifter-board/shifter-board.kicad_pcb`, generated by `tools/make_shifter_pcb.py` (run with KiCad's bundled python.exe). 50 x 54 mm, two layers, all through-hole: DIP-14 socket in the middle, six-pin header for the ESP32 leads on the left edge, four 3-pin output headers down the right edge (pin 1 +5V, 2 data, 3 GND, top to bottom), 1000 uF and the 2-pin USB input along the bottom, four M3 corner holes. GND is a copper pour on the back; +5V is a 0.8 mm track round the left, top and right edges; data tracks 0.4 mm, with the two left-column outputs and two of the inputs passing between the chip's pads on the back layer. Design rules check with schematic parity: zero violations, zero unconnected pads. Renders: `docs/diagrams/shifter-board-3d.png`, `shifter-board-top.png`; copper and silk plot `shifter-board-layout.pdf`. Revised the same day to screw terminals: J1-J4 are Phoenix MPT 0,5/3-2.54 three-way blocks (top to bottom GND, data, +5V, wire entry facing the board edge) and J5 a two-way MPT 0,5/2-2.54; the ESP32 leads keep a pin header. Board grew to 52 x 56 mm, resistors moved to x = 30, +5V bus at 2 mm from the right edge feeding each block's bottom pin from below. DRC with schematic parity: zero violations, zero unconnected pads. Gerbers and drill file in `hardware/shifter-board/fab/`, zipped as `hardware/shifter-board/shifter-board-gerbers.zip` for upload. **Ordered 7 Nov 2026:** three boards from OSH Park (expect 9 to 12 days) and the Hxchen 2.54 mm screw terminals from Amazon, 3-pin B07WR8J1RP and 2-pin B07WKL6MRM. Decision: the printed board replaces the perfboard build; perfboard four is cancelled unless the boards are wrong.
- **7 Nov 2026, perfboard four planned in KiCad.** Decision revised the same day: build perfboard four while the printed boards are in transit, as practice and as a spare; whichever works first goes in the box. The plan is a second KiCad project, `hardware/perfboard-4/`, sharing the schematic (`tools/make_shifter_schematic.py --perfboard`) with a PCB that models the hand wiring on the ELEGOO 5 x 7 cm board (`tools/make_perfboard_plan.py`): pads are holes with a lead in them, vias are holes with a jumper end, front-copper tracks are insulated jumpers on the front, back-copper tracks are bent-over tails and the two bare rails. Layout: notch up, pins 1-7 down column 09 rows H-N, 14-8 down column 12; GND rail column 01 and 5V rail column 18, rows D-V, soldered last; every socket-pin connection enters the hole beside the pin (column 08 or 13) and bends over to the pin on the back; R1 J08-J05, R2 M08-M05, R3 K13-K16, R4 N13-Q13 standing down; pigtails J1 J18/J04/J01, J2 M18/M04/M01, J3 K18/J16/L01, J4 O18/Q12/O01; ESP leads D18, D01, I08 (GPIO4), L08 (GPIO5), J13 (GPIO6), M13 (GPIO7); USB U01/U18; C1 V18/V16 with a V15-V01 jumper. Pins 10 and 13 reach the GND rail by jumpers routed round the right of the chip and under it (I13-I17-T17-T01 and L13-L14-R14-R01) because pin 14's 5V jumper blocks the way over the top. ERC 0 errors; DRC with schematic parity 0 violations, 0 unconnected (courtyard, silkscreen and library-mismatch checks set to ignore in the project, they are about manufactured boards). Plot `docs/diagrams/perfboard-4-plan.pdf`, renders `perfboard-4-front.png` and `perfboard-4-back.png`, build guide `docs/perfboard-4-build.md`. Also corrected the pinout table in bench-session-1.md: pin 12 is GPIO 6 (output 3 on pin 11, left drape) and pin 9 is GPIO 7 (output 4 on pin 8, right drape), as in the schematic.
- **8 Nov 2026, perfboard four build, steps 1 to 3 done.** Socket in (fourteen neighbour pairs silent), R1-R4 with the four pigtail greens at J04, M04, J16, Q12 (each pin to green about 220 ohm, no cross-talk), four GPIO leads in: GPIO 4 yellow I08, GPIO 5 brown L08, GPIO 6 green J13, GPIO 7 blue M13. Lead colour convention from here: yellow GPIO 4, brown GPIO 5, green GPIO 6, blue GPIO 7, red 5V, black GND.
- **8 Nov 2026, perfboard four steps 4 to 7 done.** Six chip jumpers (black GND, red 5V), four pigtail red/white pairs, ESP power halves at D18/D01, USB cable in with a bus-wire loop through W08/W11 as strain relief. USB black was soldered into V01 instead of U01; left in place (same rail), plan changed so the C1 GND jumper runs V15-U15-U01. Plan regenerated, DRC with parity still clean.
- **9 Nov 2026, perfboard four step 8 done, terminals delivered.** C1 in at V18/V16 with the V15-U15-U01 jumper, tests pass. USB joints verified live: 5.08 V across U18/V01 from the bank (the Maixbomr plug shell is not grounded, so no shell beeper test). Hxchen 2.54 mm screw terminals, 3-pin and 2-pin, delivered; OSH Park boards still in transit.
- **9 Nov 2026, perfboard four wired, step 9 done.** Both rails on (GND column 01 D-V, 5V column 18 D-V). Full test with the chip out: rails isolated from each other, every GND and 5V point on its rail, each output about 220 ohm pin to green and isolated from the rails, each GPIO lead on its own pin only. Live 5 V check about 5.0 V, chip in (notch to row G), rails still about 5.0 V with the chip in. **Perfboard four is complete**, built in one sitting over 8-9 Nov with no faults found at any step. ESP32 then connected (DevKitC-1 pinout: GPIO 4-7 are the four pins after RST on the 5V side, red on 5V, black on the GND beside it) and powered from the board: 5.06 V, 0.14 A on the USB meter, WLED answering at **http://192.168.1.34/** on home WiFi, onboard pixel follows colour changes. Next: first strip, first light.
- **11 Nov 2026, FIRST LIGHT through the shifter board.** Lapel strip one: 48 LEDs cut from the BTF WS2812B roll, male JST-SM pigtail soldered to the DI end (convention from here: board side female, every strip and string male). WLED output 2: WS281x, GRB, start 1, length 48, GPIO 4, limiter 1500 mA. Strip lit on bank power through perfboard four, output 1. Fault on the way: strip dark, rail and strip both reading 1.08 V while WLED ran normally. Cause: the bank was plugged into the ESP32's own USB socket, not the perfboard's soldered USB cable, so the board was unpowered and the rail was being back-fed through the signal leads. Lessons: (1) a rail near 1 V with nothing warm means unpowered and back-fed, check where the power enters first; (2) the beeper sounds for anything under about 50 ohm, use the ohm range to judge a power path (good paths read 0.1-0.2 ohm over the lead resistance); (3) DI to DO across an LED never beeps, the data passes through each LED's chip; (4) a brief beep that fades between 5V and GND is the 1000 uF capacitor charging, not a short; (5) the ESP32's USB sockets stay empty once it is powered from the board. An earlier 40-LED test piece with a female plug was retired as practice scrap.
- **11 Nov 2026, bench session 1 complete.** Colours correct on GRB. USB meter, whole system on the bank: ESP32 alone 0.14 A; white with the limiter at 1500 mA, 1.00 A at brightness 128 and 1.11 A at 255 (WLED reports its estimate pinned at 1500 mA, so real draw runs about a third under the WLED figure; the slider has no visible effect above about half because the limiter is already capping); Rainbow for 15 minutes 0.48 A, nothing warm at the chip, the strip joint or the USB wires. The 0.7 s fade between colours is WLED's default transition. Next: second lapel strip on output 2 (GPIO 5, brown lead), printed boards when they arrive.
- **11 Nov 2026, network facts.** Home router is a Spectrum router at 192.168.1.1; it has no web admin, settings are in the My Spectrum phone app (Services, Internet, Advanced Settings, IP reservations; menu names unverified). The ESP32 shows as **WLED**, MAC **8C:BF:EA:17:D7:E0**, 192.168.1.34. **Reserved 11 Nov 2026** in the My Spectrum app: Services, router, Advanced Settings, Port Forwarding & IP Reservations; the device is listed under its maker's name, Espressif. No port forwarding is set and none should be.
- **12 Nov 2026, second strip on output 2.** Strip two: 48 LEDs, male pigtail on the DI end, heat shrink slid on before soldering. Unpowered checks passed (5V to GND at the plug 5.5 megohm, rails under 1 ohm end to end). WLED output 3 added: WS281x, GRB, 48 LEDs, GPIO 5. Both strips show correct colours and Wipe runs strip one then strip two. USB meter at brightness 128 on Wipe: 0.20 A all dark, 0.79 A all lit. Board outputs 1 and 2 are now proven with LEDs; outputs 3 and 4 are proven by meter only.
- **12 Nov 2026, printed controller box started.** The bought 90 x 70 x 28 mm box is too small for a board plus the ESP32. Replacement is a printed tray and screw lid, `hardware/enclosure/controller-box.scad` (OpenSCAD 2021.01), 97 x 91 x 34 mm outside, boards side by side, antenna at the far wall, cables out of five labelled slots at the capacitor end, no USB opening (lift the ESP32 out to reflash by wire). Measured: perfboard 69.8 x 50.06 mm, mounting holes 66 x 46 mm centres, 2 mm; ESP32 board 57.65 x 28.24 mm plus 6.5 mm antenna overhang; heights 21.5 mm (perfboard) and 20 mm (ESP32 with plugs). Fit plate two passed for both boards; M3 screws cut their own thread in a 2.8 mm hole. First full print in PLA under way. Lesson: measure hole spacing outside-edge and inside-edge and average them.
- **12 Nov 2026, output 4 proven, output 3 fault found.** Strip two on pigtail four with GPIO 7: lit, Wipe correct. On pigtail three with GPIO 6: dark. Traced with the meter, power on, Rainbow running, black probe on D01: J08 (working output 1) 0.126 V, K13 (R3 chip side) 0.126 V, K16 (R3 far side) 0 V; strip far end 5.21 V, so power was fine. R3 measures 220 ohm. Cause: R3's far leg was bent over to pigtail three's green wire but never soldered, only touching, so it passed the build's beeper test and failed later. The green wire is in L16, not J16 as planned; the plan, the KiCad file and the build guide now show L16. Lessons: compare a dead channel against a working one, hole by hole; a joint that only touches can pass a continuity test; WLED GPIO changes do nothing until Save.
- **12 Nov 2026, output 3 repaired, all four outputs proven with LEDs.** R3's far leg soldered at K16 (anchor) and to the green wire at L16. On power-up both strips were dark with the USB meter at 0.19 A (ESP32 plus two idle strips: power present, no data): the SN74AHCT125N had dropped out of its socket while the board was face down. Its legs were bent, so a fresh chip went in, notch toward row G. Strip two then lit on pigtail three with GPIO 6, Wipe correct on both strips. Lessons: every output dark at once with normal idle current means look at the chip first; seat the chip until it is flat on the socket; the box lid should get a post over the chip. Also delivered today: hot glue gun and a second roll of the same WS2812B strip.
- **12 Nov 2026, first lapel chain: two strips of 48 on one output.** Female JST-SM pigtail soldered to the DO end of strip one (5V pad only a sliver of copper after an uneven cut; plug-to-plug 5V and GND both well under 1 ohm and steady when wiggled), strip two plugged into it. WLED: GPIO 4 output set to 96 LEDs, third output removed. Wipe runs through the join cleanly, colours correct. White at brightness 128 with the 1500 mA limiter: USB meter a little over 1 A, 4.63 V at the far end of strip two, against 5.21 V with the strips idle. About 0.6 V of droop at 1 A through one feed point; acceptable here, and it confirms the spec's far-end power feed will be needed at the planned 2.5 A.
- **12 Nov 2026, printed controller box, first full print passed.** PLA, 0.2 mm layers, no supports. Both boards seat, lid posts land on clear spots, lid closes flat, cables sit in their slots under the tongues, M3 x 10 pan-head screws hand-tight, no rattle when shaken, both strips light with the lid screwed down. To add before the PETG print: a lid post over the level-shifter chip (it fell out of its socket once today), the microphone port, and the button wiring exit. Designer note updated to the new size.
- **12 Nov 2026, first lapel set up as a 2 x 48 panel.** WLED outputs reordered so the strips come first: output 1 GPIO 4, 96 LEDs, start 0; output 2 the onboard pixel, GPIO 48, 1 LED, start 96 (WLED builds a panel from the first LEDs in the list). 2D Configuration: 2D Matrix, one panel, first LED top left, vertical, serpentine, 2 wide by 48 high. Segment setting Expand 1D FX = Bar, so line effects move both columns together; with the default (Pixels) the left column leads the right by one step. Wipe pulls down level. WLED 16 notes: the GPIO box is a drop-down that greys out pins in use, so to swap two outputs remove one, save, change the other, save, then add the first back; segment settings such as Bar are lost on restart until saved in a preset.
- **13 Nov 2026, boot preset saved.** Preset 1 "Lapel test": Wipe, brightness 128, segment 0 with Expand 1D FX set to Bar. The first save kept Pixels because the drop-down inside the segment had not been changed; fixed by setting Bar, then pencil, "Use current state", save. "Apply preset 1 at boot" is on the LED & Hardware page in WLED 16 (older guides call it LED Preferences). Survived a power cycle: both columns move together.
- **13 Nov 2026, microphone pins soldered.** INMP441 from the AITRIP 3-pack: bare board with two loose 3-pin strips. Pins fitted with the long ends on the can side, soldered on the flat side with tape over the sound hole. Beeper silent between neighbouring pins; VDD to GND not near zero. Rows read L/R, WS, SCK and SD, VDD, GND. ESP32 header after pin 7 reads 15, 16, 17. Wiring waits for socket-to-socket jumpers (due 14 Nov): VDD to 3V3, GND and L/R each to a GND pin, SD 15, WS 16, SCK 17. The 3-pin LED pigtail that came in the pack cannot carry the microphone; kept for a second power feed.
- **13 Nov 2026, chip hold-down added to the box lid model.** Measured 7 mm from the board surface to the top of the chip. `controller-box.scad` now has a blade under the lid (17 x 3 mm, 1 mm above the chip, 9 mm notch in the middle for the GND lead that crosses the chip), placed from the hole map: chip centre 3.81 mm toward the antenna end and 2.54 mm toward the ESP32 from the board centre. Lid is one watertight body, no overlap with the tray. Printed in the evening and fitted: lid sits, lights on.
- **13 Nov 2026, lapel one on the mock cape.** Both strips of lapel one pinned onto the Halloween cape on the torso stand, running from the boot preset. Reported working. CME WIDI Master ordered the same day for phase 2; agreed to start the MIDI firmware with the five mappings in section 6.
- **14 Nov 2026, microphone wired, not yet heard.** Jumpers and the WIDI Master arrived. INMP441 wired with six jumpers: VDD to 3V3, GND and L/R to GND pins, SD 15, WS 16, SCK 17. Beeper checks passed on power and on all three signal leads. AudioReactive enabled in WLED 16.0.1 (Usermods page: Generic I2S, pins 15, 16, 17) and the controller restarted. Sound effects stay dark: WLED reports the source as "I2S digital - quiet" even with squelch 0 and gain 255, so the ESP32 receives silence. L/R moved to 3V3 as a channel test: no change. Next: DC volts at the mic pins while powered (VDD 3.3 V, SCK and WS about 1.6 V as the average of a running clock) to tell a missing clock from a dead or badly joined mic board.
- **15 Nov 2026, microphone working.** With L/R back on GND, DC volts at the mic pins while powered: VDD about 3 V, SCK about 1.5 V, WS about 1.5 V (running clocks read as their average), SD 0.5 V. WLED's pin table showed 15, 16, 17 owned by the usermod. Cause was the mic board's own joints: soldered too lightly on 13 Nov, no cone onto the pads, so the pins were not joined to the board. The beeper could not show this because the probe touched the pin, which is the same metal as the lead. All six joints re-melted with fresh solder; Gravimeter now follows voice and finger snaps. Settings at this point: Generic I2S, squelch 10, gain 60, AGC off. Bench tuning the same day: at gain 60 a mouse click on the desk lit the whole lapel and speech pinned it; gain lowered to 10 (squelch 10, AGC off) gives a usable range for voice at the bench. To be redone at rehearsal volume.

## 8. Revised electronics bill of materials

Quantities include spares where strips and strings come in fixed lengths. Prices are retail USD, rough.

| Part | Qty | Est. | Notes |
|---|---|---|---|
| WS2812B strip, 5V, 60 px/m, IP65 silicone sleeve | 5 m roll | $22-32 | Need ~3.6 m for B and C, roll gives spares |
| WS2812B COB strip, 5V, ~160 px/m, IP65 | 2 m | $24-36 | Need ~1.1 m for the collar edges and rim, plus spare for the corner cuts |
| WS2811 12 mm bullet pixel string, 5V, 10 cm spacing | 2 × 50 px | $30 (2-pack) | Need 40 at the stock 10 cm spacing; one string per side with slack, leftovers are repair stock |
| Clear silicone sleeve for the COB strip, 5-6 mm | 2 m | $8 | 5V COB is only sold bare; the collar is a sweat zone |
| ESP32-S3 dev board, 8 MB flash + PSRAM | 1 | $12-20 | |
| INMP441 I2S microphone | 1 | $5-8 | |
| SN74AHCT125N + DIP socket, or breakout | 1 | $3-8 | |
| USB power bank, 20,000 mAh, always-on mode | 1 | $40-60 | Section 4.2 criteria; 10,000 mAh variant $25-40 |
| USB-A to bare-wire cable, 20 AWG | 2 | $8-12 | One spare |
| Inline fuse holder + 3 A fast fuses (5) | 1 set | $6-10 | |
| Rocker or toggle switch, 5 A | 1 | $5-8 | |
| Perfboard, screw terminals or WAGO 221 lever connectors | 1 set | $6-12 | Distribution block |
| 1000 uF 16 V capacitors (2), 220 ohm resistors (10), 0.1 uF (5) | 1 kit | $5-8 | |
| Silicone wire, 20 AWG red and black, 5 m each | 1 | $10-14 | |
| Silicone wire, 24 AWG, 5 m, for data | 1 | $4-6 | |
| JST-SM 3-pin connector pairs, 22 AWG | 10 | $8-12 | |
| JST-SM 2-pin or XT30 pairs | 4 | $4-8 | Lapel power injection |
| 12 mm momentary panel-mount buttons | 3 | $5-10 | |
| ABS project box, ~80 × 50 × 25 mm | 1 | $5-10 | |
| Heat shrink assortment, braided sleeving 3 m | 1 | $8-12 | |
| USB inline current meter | 1 | $8-12 | For testing; you will use it every build |
| Waist pouch for electronics | 1 | $8-15 | Already in the brief |
| **Base electronics subtotal** | | **$215-335** | Up from $145-255 because the pixel count tripled |
| Phase 2: CME WIDI Master | 1 | $50-65 | Plugs into the Kronos MIDI OUT, bus-powered; BLE MIDI to the cape |
| Phase 2 fallback: MIDI DIN jack + 6N138 + passives | 1 | $5-10 | Wired alternative if BLE pairing is flaky in venues |
| Foam mic windscreen, small | 2 | $3-5 | Over the INMP441 port on the box face |
| Not in the brief's BOM: 3D-printed epaulet shells (page 6 of the brief) | 2 | $10-30 | Filament or a print service; costume side |

Tools assumed on hand: soldering iron, multimeter, wire strippers, heat gun or lighter. Add $40-80 if you need to buy these.

## 9. Open decisions for the first fitting

- **Decided 20 Oct 2026: two columns per lapel** (section 2, zone B), wired serpentine and configured as a 2 x 48 matrix per lapel in WLED. Confirm with a bench look at one versus two columns under the diffuser, and check that the designer's lapel facing is at least 50 mm wide. If the lapel ends up under 35 mm, fall back to one column. If a 5V double-row 120 px/m strip is available, it replaces the two separate columns as a single part.
- **Decided 20 Oct 2026: COB for the collar**, as a lit outline along the two straight front edges and, per the later render review the same day, continuing along the full top rim around the back of the neck, split at centre back. Order 2 m of IP65, 5V, WS2812B-protocol COB and confirm the pixels-per-metre figure on the listing. Cover with black mesh so the phosphor stripe is hidden when off. Keep 60 px/m from the lapel roll as the fallback if the COB disappoints on the bench (front edges only in that case).
- **Added 20 Oct 2026 from the render review:** side-panel bullets (zone E) run along the outer side seams rather than scattered across the panels, and no bullet in zones D or E sits within 15 cm of the hem.
- **Added 20 Oct 2026 from the parts research (docs/shopping-list.md):** 5V 12 mm bullet strings ship with 10 cm spacing, so zones D and E are 8 and 12 bullets per side (40 total, ~430 pixels overall). No 5V COB strip is sold waterproofed, so the collar COB gets a clear silicone sleeve. The 20,000 mAh bank with a documented always-on mode is not reliably in stock on Amazon; the shopping list recommends two Anker PowerCore Slim 10000 banks (documented trickle mode, 5V/3A USB-A) instead, pending Mike's decision.
- **Added 20 Oct 2026 from the lights-off zone-map render:** each front edge gets one continuous 28-32 mm mesh-faced channel from the collar to 15 cm above the hem. Zone B (two strips) occupies the top 80 cm, zone D (bullet string) sits loose in the rest of the same channel, no grommets on the front edges. The waist connector for zone D exits through a small opening on the channel's inside face.
- **Decided 20 Oct 2026: shoulders lit**, on the condition that the epaulets are soft or semi-rigid (EVA foam or stiff interfacing shaped at the fitting), not hard 3D-printed shells, and that the reach test at both keyboards passes with foam epaulet mockups at the plain-fabric fitting. The shoulder strip is the last segment of each front chain, so dropping it later changes nothing else. Reason: from behind two keyboards the audience sees collar, shoulders, and upper lapels; the lower zones are partly hidden by the instruments.
- **Decided 20 Oct 2026: 20,000 mAh bank.** The margin covers a forgotten charge, cell aging, and a higher cap later. Choose the model by its always-on or low-current mode first (Anker trickle-charging mode, INIU low current mode, Voltaic always-on), confirmed in the manual, with a USB-A port rated 2.4 A or more. Give the designer the bank's dimensions before the fitting; if the pouch cannot be concealed at the waist, drop to the 10,000 mAh version of the same line and nothing else changes.
- **Decided 20 Oct 2026: mic on the outward face of the controller box or pouch at the hip**, port through an open-mesh patch with a foam windscreen, lead under 15 cm. The designer needs to know the pouch sits on the belt at the hip with that face exposed, not buried under the front panels. Fallback if the hip cannot be exposed: under the epaulet edge at the shoulder, sharing the front-quadrant connector path. Not the collar (long I2S run, sweat, rubbing noise). The mic stays in the phase 2 show as the band layer under the Kronos MIDI layer (section 6).

All five open decisions are now closed. Sections 2, 3, 5, 6 and 8 reflect them.
