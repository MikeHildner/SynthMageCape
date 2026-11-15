# Perfboard Four: Build Guide With KiCad Alongside

Board four is the same shifter and distribution circuit as the printed board, hand wired on an ELEGOO 5 x 7 cm perfboard. The difference from board three is that this one was planned hole by hole in KiCad first, and KiCad checked the plan against the schematic before any solder went on. The build follows the plan, and at each step you use KiCad to see what you are about to do and the meter to confirm you did it.

Files:

- `hardware/perfboard-4/perfboard-4.kicad_pro`: open this in KiCad 10. The schematic is the same circuit as the printed board (written by `tools/make_shifter_schematic.py --perfboard`); the PCB is the perfboard plan (written by `tools/make_perfboard_plan.py`).
- `diagrams/perfboard-4-plan.pdf`: the plan on paper, front copper, back copper, silkscreen and the empty holes.
- `diagrams/perfboard-4-front.png` and `perfboard-4-back.png`: 3D renders. The back render is mirrored, exactly as the board looks when you flip it over.

Checks the plan passed: electrical rules on the schematic (0 errors), design rules on the board with schematic parity (0 violations, 0 unconnected, 0 footprint errors). Parity means every pad on the board carries the net the schematic says it should.

## 1. How to read the plan

The board is held with the numbers 01 to 18 across the top and the letters A to X down the sides. A hole is letter then number: P12 is row P, column 12. The silkscreen in KiCad carries the same labels, so what you see on screen is what you see on the board.

KiCad draws printed boards, so the plan borrows its vocabulary:

| On screen | On the board |
|---|---|
| Pad (yellow ring) | A hole with a component leg or a loose wire in it |
| Via (grey ring) | A plain hole with the end of an insulated jumper pushed through it |
| Red track (F.Cu, front copper) | An insulated solid-core jumper lying on the front |
| Blue track (B.Cu, back copper) | Bare metal on the back: a leg bent over to the next hole, or the bus wire of a rail |
| Small circle (User.Drawings layer) | A hole that stays empty |

Two rules from board three are built in. The 5 V rail (column 18) and the GND rail (column 01) are on opposite edges, so no bare lead can reach both. And every connection to a socket pin is made through the hole beside the pin (column 08 on the left, column 13 on the right): the wire or leg goes through its own hole from the front, and its tail is bent over to the pin on the back. That holds the wire still while you solder, which is what board three's pin joints lacked.

Layout at a glance (notch at the top):

- U1 socket: pins 1 to 7 down column 09, rows H to N; pins 14 to 8 down column 12, rows H to N.
- R1 (pin 3) J08 to J05, R2 (pin 6) M08 to M05, R3 (pin 11) K13 to K16, R4 (pin 8) N13 down to Q13.
- Pigtails: J1 red J18, green J04, white J01. J2 red M18, green M04, white M01. J3 red K18, green L16, white L01. J4 red O18, green Q12, white O01.
- ESP32 leads: 5 V D18, GND D01, GPIO 4 I08, GPIO 5 L08, GPIO 6 J13, GPIO 7 M13.
- USB in: black V01, red U18. C1: long leg V18, stripe leg V16, with a jumper V15 to U01.
- Rails: bare wire on the back along column 01 and column 18, rows D to V, soldered last.

## 2. KiCad first: ten minutes before the iron is on

Open `perfboard-4.kicad_pro`, then double-click the PCB editor.

1. **Appearance panel** (right side, Layers tab). Tick and untick F.Cu, B.Cu and User.Drawings and watch the front jumpers, the back bends and the empty holes come and go. The eye icons are visibility, the colour swatches tell you which layer a track is on.
2. **Click a pad.** The message panel, the strip just above the status bar, reads something like `Pad 7 [GND] of U1`. If the click selects the whole socket, click empty board, then click again right on the ring. The part in brackets is the net: the set of everything that must end up electrically joined. The meter test for any step is "does everything on this net beep together, and nothing else".
3. **Highlight a net.** Hover a pad and press the backtick key (left of the 1 key). Every pad, via and track on that net lights up and the rest dims. Try GND: you will see the column 01 rail track, the three left pins, the two right pins with their long jumpers, the four white pigtail holes, the USB black hole, the ESP GND hole and the capacitor's jumper. Press Escape to clear.
4. **3D viewer**, View menu or Alt+3. Drag to rotate, scroll to zoom. Flip it over to see the back as you will see it on the bench.
5. **Nets tab.** In the Appearance panel, next to Layers and Objects, the Nets tab lists every net with a highlight icon on the left. Clicking it does what the backtick does. (KiCad 10 has no Net Inspector under the Inspect menu; this tab is where the list lives.)
6. **Schematic to board.** Open the schematic editor from the project window, click any pin of U1, and look at the PCB editor: the same pad is highlighted there. That link is what "parity" checked.

## 3. Materials

Everything is already on hand except that you need four pigtails, not one:

- ELEGOO 5 x 7 cm perfboard, DIP-14 socket, SN74AHCT125N, four 220 ohm resistors, 1000 uF capacitor.
- Four JST-SM 3-pin **female** pigtails (red, green, white wires).
- Six Dupont half-leads for the ESP32: salvage the yellow, brown, red and black halves from board three by snipping them off at the board, and cut one more jumper in half for GPIO 6 and GPIO 7. Board four used green for GPIO 6 and blue for GPIO 7.
- Solid-core 22 AWG insulated wire, seven jumpers: three of 25 mm, one of 25 mm for 5 V, one of 90 mm, one of 60 mm, one of 45 mm.
- Bare tinned bus wire, two pieces of 50 mm, for the rails.
- The USB-A to bare-wire cable.

Stripping rule for every wire: 4 mm at an end whose tail has to bend over to a neighbouring hole, 3 mm at an end that only sits in its own hole. Nothing bare longer than that anywhere.

## 4. Build steps

After each step, do the KiCad look and the meter test before moving on. If a test fails, stop and find it while there are only a few joints to suspect.

### Step 1: socket

Push the socket in from the front with the notch toward row G (the top). Pins land in H09 to N09 and H12 to N12. Solder H09 and N12 first, press the socket flat while the second one cools, then the other twelve. Chip stays in its bag.

KiCad: hover U1's pads. Pin 1 is at H09 and shows `[GND]`, pin 14 at H12 shows `[+5V]`. Note which pins are inputs (I09, L09 on the left; J12, M12 on the right) and which are outputs (J09, M09; K12, N12).

Meter, continuity: probe neighbouring pins on the back, H09 to I09, I09 to J09, and so on down both sides. Silence everywhere. A beep is a bridge; wick it now.

### Step 2: resistors and the four green wires

Do each output the same way. For output 1: push the green wire of pigtail one through J04 from the front, 3 mm bare. Push R1's legs through J08 and J05 from the front. On the back, bend the J08 tail flat toward J09 and solder it onto pin 3's joint. Bend the J05 tail toward J04 and solder it to the green wire's tail and the J04 ring in one joint. Trim.

The other three: R2 M08 to M05 with pin 6 at M09 and green at M04. R3 K13 to K16 with pin 11 at K12 and green at L16 (the tail bends down one row, not sideways; planned as J16, built as L16). R4 stands downward, N13 to Q13, with pin 8 at N12 and green at Q12 (the tail bends left one hole).

KiCad: highlight `Net-(J1-Pin_2)`, which is the output 1 data net. Only three things light: R1's far pad, the green pad J04 and the short blue bend between them. Highlight `Net-(R1-Pad1)`: pin 3 and R1's near pad.

Meter, ohms: pin 3 (J09, on the back) to the green wire's plug end reads about 220. Same for pin 6 to green 2, pin 11 to green 3, pin 8 to green 4. Continuity between any green and any socket pin other than its own: silent.

### Step 3: ESP32 signal leads

Four Dupont halves, 4 mm stripped and tinned so the strands stay together. Through from the front: GPIO 4 lead (yellow) into I08, tail bent to pin 2 at I09. GPIO 5 (brown) into L08, tail to pin 5 at L09. GPIO 6 (green) into J13, tail to pin 12 at J12. GPIO 7 (blue) into M13, tail to pin 9 at M12.

KiCad: highlight `/GPIO6`. It lights J6's pad at J13, the bend, and pin 12. Note it is pin 12, not pin 9: the schematic uses buffer D for GPIO 6 and buffer C for GPIO 7.

Meter, continuity: each lead's plug end beeps to its own pin and to nothing else.

### Step 4: the chip's power and enable jumpers

Six insulated jumpers, pin end 4 mm bare and bent to the pin, rail end 3 mm bare and left in its hole for now.

| Jumper | From (pin end) | Bend to | Route on the front | To (rail end) | Cut length |
|---|---|---|---|---|---|
| Pin 1 GND | H08 | H09 | straight along row H | H01 | 25 mm |
| Pin 4 GND | K08 | K09 | straight along row K | K01 | 25 mm |
| Pin 7 GND | N08 | N09 | straight along row N | N01 | 25 mm |
| Pin 14 5 V | H13 | H12 | straight along row H | H18 | 25 mm |
| Pin 13 GND | I13 | I12 | right to column 17, down to row T, left | T01 | 90 mm |
| Pin 10 GND | L13 | L12 | right to column 14, down to row R, left | R01 | 60 mm |

The two long ones go round the right and under the chip because pin 14 (5 V) sits between pin 13 and the top edge, and the two routes must not cross on the same side of the board. Lay the wire flat along the path shown in KiCad; the bends are just bends, no holes.

KiCad: highlight `+5V`, then `GND`. Every lit hole is a place the rail wire will touch in step 9. The two long GND jumpers are the red tracks that go round R4.

Meter, continuity: the five GND pins do not beep to each other yet, the rail is not on. Each one beeps to its own rail-end hole (H01, K01, N01, R01, T01), and H12 beeps to H18. 5 V to GND silent: H18 to any of the GND rail holes.

### Step 5: pigtail power wires

Each pigtail's red into its column 18 hole, white into its column 01 hole, 3 mm bare, solder in place: J1 J18 and J01, J2 M18 and M01, J3 K18 and L01, J4 O18 and O01. Keep the pigtail bodies out of the way with a twist of tape to the board edge.

Meter: the reds do not beep to each other until the rail is on. Each red beeps to its own hole and is silent to its white.

### Step 6: ESP32 power leads

Red half into D18, black half into D01, 3 mm bare, solder.

### Step 7: USB cable

Black into V01, red into U18. Strain relief first: a 6 cm piece of bare bus wire bent into a U, legs down through W08 and W11 over the cable's jacket, twisted together on the back and trimmed. Zip ties do not fit perfboard holes. Test: with nothing else connected, cable in the bank, DC volts red probe U18, black probe V01, about 5 V; unplug. The plug shell on the Maixbomr cable is not wired to GND, so a beeper test to the shell says nothing.

### Step 8: capacitor

Long leg (positive) into V18, stripe leg into V16, from the front. On the back, bend the V16 tail toward V15. Push a 45 mm jumper through V15 from the front, 4 mm bare, and solder its tail together with the capacitor's bent leg into V15. The other end runs up one row and left along row U to U01, 3 mm bare, solder.

Backwards damages the capacitor: the stripe is the negative side and it must end up on the GND rail. Check before soldering.

### Step 9: rails, last

Lay a 50 mm piece of bare bus wire down column 01 on the back from D01 to V01 and tack it at both ends. Then reflow every joint it crosses so the wire is in the solder: D, H, J, K, L, M, N, O, R, T, U, V (U is the capacitor jumper, V the USB black). The empty rings between can take a touch of solder too. Repeat down column 18: D, H, J, K, M, O, U, V.

KiCad: this is the moment the two blue column tracks become real. Highlight GND once more and read off the list of rail joints.

Meter, continuity, with the chip still out:

1. 5 V rail to GND rail (D18 to D01): silent. If it beeps, the fault is somewhere in this step or step 8.
2. Every GND point to D01: pins 1, 4, 7, 10, 13, all four whites, USB black at V01, ESP black, capacitor stripe leg. Beep.
3. Every 5 V point to D18: pin 14, all four reds, USB red, ESP red, capacitor long leg. Beep.
4. Each green plug to its own socket pin: about 220 ohm, and silent to the rails.

### Step 10: power and chip

USB into the bank, meter on DC volts across D18 and D01: about 5.0 V. Unplug. Insert the chip, notch to the top matching the socket, press evenly. Power always enters through the USB cable soldered to this board (U18 and V01); the ESP32's own USB sockets stay empty. If the bank is plugged into the ESP32 instead, WLED runs but the rail sits near 1 V and every strip stays dark. Strips carry the male JST-SM plug, the board's pigtails are female. Then first light per bench session 1, sections 4 and 5: output 1 is pigtail one, GPIO 4, WLED output 2, 48 LEDs, 1500 mA limiter.

## 5. If a test fails

Find the hole name of the two probe points, open the PCB in KiCad and highlight the net of one of them. Everything that should beep is lit. If the other probe point is lit and it is silent, a joint on the lit path is open: look at each one with the loupe. If the other point is not lit and it beeps, two nets are touching: look at every place the two highlighted sets come within one hole of each other, those are the suspects.
