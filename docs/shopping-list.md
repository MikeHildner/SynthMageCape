# Synth Mage Cape: Amazon Shopping List

Compiled 20 October 2026 from live Amazon.com listings. Prices and stock were read that day and will drift. Before checkout, confirm on each page the one spec point noted in the "Check" column; Amazon listings change variants without changing the URL.

Core order (sections A, B, C) is about $310. Optional lines add about $90.

## Decisions baked into this list

Three things changed from the spec once real listings were checked:

1. **Bullet pixels come with 10 cm spacing, not 5 cm.** That is what every 5V 12 mm string on Amazon ships with. At 10 cm, the lower front edge (75 cm) takes 8 bullets and the outer side seam (about 1.2 m) takes 12, so 20 per side and 40 total instead of 80. The 2-pack of 50-bullet strings still makes sense: one string per side with slack, and the leftover bullets are repair stock.
2. **No 5V COB strip is sold with a waterproof sleeve.** All of them are bare (IP30). The collar sits in a sweat zone, so the COB gets a clear silicone sleeve added on the bench. That is one extra line below.
3. **A 20,000 mAh bank with a documented always-on mode is not reliably available on Amazon right now.** The Anker 20K models that document trickle mode are out of stock. The Baseus and NOBIS 20K banks advertise a low-current mode but do not state the USB-A amp rating in the listing text. The one bank that documents both the mode and a 5V/3A USB-A port is the Anker PowerCore Slim 10000. Recommendation: **buy two of them.** Each runs the cape for 2.5 hours at the cap and 5 hours at realistic draw. One lives in the pouch, the other stays charged as the swap, which covers the forgot-to-charge case better than a single 20K. Total cost is about the same. If you would rather have the single 20K as decided, the Voltaic V75 from the manufacturer is the always-on bank built for this, at a higher price.

## A. LED strips and pixels

| Item | Pick | Link | Price | Check |
|---|---|---|---|---|
| WS2812B strip, 5V, 60 px/m, 5 m, IP65, black PCB | BTF-LIGHTING 16.4FT 300LED IP65 Black | https://www.amazon.com/dp/B088B9QWHT | $22.99 | Select **Black IP65** and **16.4FT 300LED**. Verified 5V, 60/m, 10 mm black PCB, JST-SM both ends. |
| COB strip, 5V, 160 px/m, 2 m, 5 mm wide | BTF-LIGHTING FCOB WS2812B 5V | https://www.amazon.com/dp/B0CNXLHPTT | about $18 | Select **6.5FT** and **5mm** on this listing. One IC per LED, individually addressable. Bare strip (IP30). Same specs, verified at $18.99: https://www.amazon.com/dp/B0CQWZZJYB |
| Clear silicone sleeve for the COB, 5 to 6 mm | not researched; search Amazon for "silicone LED strip sleeve 5mm" | | about $8 | Must slide over a 5 mm wide, 2 mm thick strip. Buy 2 m. |
| 12 mm WS2811 bullet pixels, 5V, 50 per string, black wire, 2-pack | ALITOVE 5V 12MM Black Wire IP68 | https://www.amazon.com/dp/B06XD72LYM | $30.00 (2-pack) | Select **DC 5V 50pcs (Pack of 2)**. Verified 5V, 10 cm spacing, JST-SM plus separate power leads. |

## B. Controller, sensing, power

| Item | Pick | Link | Price | Check |
|---|---|---|---|---|
| ESP32-S3-DevKitC-1 N16R8 | QIQIAZI ESP32-S3 DevKitC-1 N16R8 | https://www.amazon.com/dp/B0H7HTMGK9 | $15.99 | Listing states 16 MB flash + 8 MB PSRAM. Low stock when checked. Alternative: https://www.amazon.com/dp/B0CDRM6BGQ |
| INMP441 I2S microphone, 3-pack | AITRIP 3PCS INMP441 | https://www.amazon.com/dp/B0972XP1YS | $9.99 | Omnidirectional I2S. Spares are useful; these are fragile. |
| SN74AHCT125N level shifter, DIP-14, 10-pack | Bridgold 10pcs SN74AHCT125N | https://www.amazon.com/dp/B08R6BCSYC | $7.99 | Must say **AHCT**, not HC. Listing confirms TTL-compatible inputs. |
| Power bank with always-on mode, buy 2 | Anker PowerCore Slim 10000 (USB-A + USB-C) | https://www.amazon.com/dp/B0D5CLSMFB | $21 to $26 each | Listing documents "Trickle-Charging Mode" (double-press the button, LED turns green) and USB-A output 5V/3A. Sold by AnkerDirect. |
| Alternative single 20K bank | Voltaic V75 (always-on, 19,200 mAh), manufacturer only | https://voltaicsystems.com/always-on/ | about $100 and up | Built for exactly this; never shuts off. Not on Amazon. |
| USB-A to bare-wire cable, 22 AWG, 2-pack | Maixbomr USB-A to 2-pin bare wire | https://www.amazon.com/dp/B0C7KBKTMN | $8.99 | Select the **1 m** length variant. Power only, 5V/3A rated. |
| USB inline power meter | Eversame USB 3.0 Tester | https://www.amazon.com/dp/B01D9Y6ZFW | $13.99 | 0 to 5.1 A range. Any similar USB-A tester works. |

## C. Wiring, connectors, protection

| Item | Pick | Link | Price | Check |
|---|---|---|---|---|
| Inline fuse holders plus fuse assortment (includes 3 A) | BOJACK 10 holders + 150 glass fuses | https://www.amazon.com/dp/B0813Q4S6P | $11.99 | 5x20 mm, assortment includes 3 A fast-blow. Covers both the holder and the fuses. |
| Power switch, SPST, pre-wired, 5-pack | DaierTek KCD1 20 mm round rocker | https://www.amazon.com/dp/B07S1MV462 | $6.39 | 20 A at 12V DC, 20 mm panel hole. Toggle-with-boot alternative if you want a harder-to-bump switch: https://www.amazon.com/dp/B07WW3WW3F ($11.99) |
| JST-SM 3-pin pairs, 22 AWG, 10 pairs | WESIRI 10 pairs | https://www.amazon.com/dp/B0BW777LBK | $6.49 | Matches the connectors on the strip and the bullet strings. |
| JST-SM 2-pin pairs, 22 AWG, 10 pairs | generic 2-pin JST SM 150 mm | https://www.amazon.com/dp/B09TR3QCQQ | $6.50 | For the lapel-bottom power injection leads. |
| Silicone wire, 20 AWG, red and black, 7 m each | Haerkn 20 AWG 2-color | https://www.amazon.com/dp/B08F79YG8Q | $7.98 | High strand count, 200 C rated. |
| Silicone wire, 24 AWG, 5 colors, 6 m each | Striveday 24 AWG 5-color | https://www.amazon.com/dp/B01LH1G2IE | $14.99 | One color per quadrant data line makes debugging easier. Cheaper 2-color option: https://www.amazon.com/dp/B01CQJF3BS ($6.98) |
| 1000 uF 16V capacitors, 25-pack | uxcell 1000uF 16V | https://www.amazon.com/dp/B08MZ42BYC | $8.79 | Low stock when checked; any 1000 uF 10V to 16V electrolytic works. |
| 220 ohm 1/4 W resistors, 100-pack | Chanzon 220 ohm metal film | https://www.amazon.com/dp/B08QRFCMC9 | $5.49 | |
| DIP-14 IC sockets, 40-pack | uxcell DIP-14 socket | https://www.amazon.com/dp/B07H3WQYRQ | $6.99 | For the level shifter so it is replaceable. |
| Perfboard assortment | ELEGOO 32 pcs double-sided | https://www.amazon.com/dp/B072Z7Y19F | $9.99 | 2.54 mm pitch, plated through. |
| WAGO 221-413 lever connectors, 10-pack | Wago 221-413 | https://www.amazon.com/dp/B0719LG1TF | $8.96 | Distribution block. Three-conductor, tool-free, re-openable. |
| 12 mm momentary buttons, waterproof, 5-pack | Gebildet 12 mm prewired IP65 | https://www.amazon.com/dp/B08L49F7DV | $7.99 | Momentary, normally open, pre-wired. |
| ABS project box, screw lid, 5-pack | LeMotech 90 x 70 x 28 mm | https://www.amazon.com/dp/B0BQYMY8GL | $9.99 | Slightly larger than the spec's 80 x 50 x 25; it is the smallest well-reviewed box with a screw-down lid. Inner 82 x 65 x 24 mm. **Found too small 12 Nov 2026:** perfboard four (70 x 50 mm) and the ESP32 do not fit together, and the printed board (52 x 56 mm) beside the ESP32 would leave no room for plugs. Replaced by a printed box, 97 x 91 x 34 mm (`hardware/enclosure/`), first print passed 12 Nov 2026. The five bought boxes are spare for other projects. |
| Heat shrink assortment, 2:1, 5 colors | Eventronic 600 pcs | https://www.amazon.com/dp/B072PCQ2LW | $6.99 | Colors double as the connector color-coding. |
| Braided sleeving, 6 mm, 25 ft, black | Alex Tech 1/4 inch PET | https://www.amazon.com/dp/B071JH14WZ | $7.99 | For the two long shoulder cables. |
| Foam mic windscreens, 12-pack | Kicada mini foam, 6 to 8 mm | https://www.amazon.com/dp/B0CMTX728B | $8.88 | |

## D. Optional and later

| Item | Pick | Link | Price | When |
|---|---|---|---|---|
| CME WIDI Master (BLE MIDI from the Kronos) | CME WIDI Master | https://www.amazon.com/dp/B08F8F57JX | $59.00 (listed; price paid not recorded) | **Ordered 13 Nov 2026, due 14 Nov.** Phase 2. Bus-powered from the Kronos MIDI OUT. Only the larger plug is needed, in MIDI OUT; the smaller MIDI IN plug stays in the box. First use: pair it with a phone or tablet running a Bluetooth MIDI monitor app to confirm what the Kronos sends. |
| 6N138 optocoupler, 10-pack | Chanzon 6N138 DIP-8 | https://www.amazon.com/dp/B0DL5MKKNX | $5.99 | Only if BLE proves flaky and you want the wired MIDI fallback. |
| 5-pin DIN panel jack, 4-pack | CESS 5-pin DIN female | https://www.amazon.com/dp/B01GBT9RC0 | $7.39 | Same. |
| Black power mesh, 2 yards, 60 inch wide | Barcelonetta 4-way stretch power mesh | https://www.amazon.com/dp/B09ZXDFWJB | $14.90 | Costume side. Buy it only if the designer wants you to supply the diffusion mesh. Useful for bench tests either way. |

## E. Tools (added 24 October 2026)

Listings verified on Amazon.com on 24 October 2026. Two routes: the **all-in-one station kit** is the simplest buy for someone starting from nothing; the **Pinecil** is the better iron if you already own a 60 W or stronger USB-C laptop charger, since it ships without a power supply.

Recommended route is about $150. The budget route at the bottom is about $110. My earlier $70 to $100 guess was low; the flux and a proper temperature-controlled iron are what push it up, and both are worth it.

| Item | Pick | Link | Price | Notes |
|---|---|---|---|---|
| Soldering station kit | YIHUA 926 III 60W digital station kit (includes 2 helping hands, 6 tips, tweezers, solder sucker) | https://www.amazon.com/dp/B082F1WKP9 | $48.99 | Temperature controlled with a digital readout, sleep mode, the most-reviewed station in its class. The kit's helping hands cover that line below. Comes with a sponge only, so add the brass cleaner. |
| Iron alternative | PINECIL smart USB-C iron, sold by Pine64 | https://www.amazon.com/dp/B096X6SG13 | $39.99 | No power supply included. Needs a USB-C PD charger that offers 20 V at 3 A (a typical 65 W laptop charger). Cheaper direct from pine64.com at $25.99 plus shipping. If you go this way, add the helping hands below. |
| Brass tip cleaner | Hakko 599B-02 | https://www.amazon.com/dp/B00FZPGDLA | $9.96 | Keeps the tip working. Neither iron above includes one. |
| Solder | MAIYUM 63/37 rosin core, 0.8 mm, 50 g | https://www.amazon.com/dp/B075WB98FJ | $8.99 | Leaded, flows easily, 50 g is plenty for this build. Lead-free alternative if you prefer: AUSTOR 0.8 mm 100 g, https://www.amazon.com/dp/B01M071WEE, $17.99. |
| Flux | MG Chemicals 8341 no-clean flux paste, 10 mL syringe | https://www.amazon.com/dp/B00425FUW2 | $19.00 | The single biggest help on LED strip pads. Cheaper option with unconfirmed stock: Chip Quik SMD291 5 cc, https://www.amazon.com/dp/B09RQ4RK72, $11.89. |
| Solder wick | TOWOT 2.5 mm no-clean braid, 2-pack | https://www.amazon.com/dp/B09MVNQJ57 | $7.99 | For undoing mistakes. |
| Helping hands | NEIKO 01902 with magnifier | https://www.amazon.com/dp/B000P42O3C | $9.99 | Only needed on the Pinecil route; the YIHUA kit includes two. Step-up with four arms and a lit 3x magnifier: XYK medium, https://www.amazon.com/dp/B09DRZLM5N, $29.99. |
| Wire stripper | KAIWEETS self-adjusting, 10 to 24 AWG, with cutter and crimper | https://www.amazon.com/dp/B097SZ1F7W | $13.99 | Covers the 20 and 24 AWG wire in the build. |
| Flush cutters | KATA 5-inch precision flush cutters, 2-pack | https://www.amazon.com/dp/B0BBML9M2V | $6.99 | The classic Hakko CHP-170 is currently overpriced on Amazon with slow shipping; these do the same job. |
| Multimeter | AstroAI M4K0R auto-ranging, TRMS, 4000 counts | https://www.amazon.com/dp/B08DHHJPS1 | $18.99 | Continuity beep, 10 A DC range, auto-ranging so you never pick the wrong scale. Manual-ranging budget version: AstroAI AM33D, https://www.amazon.com/dp/B01ISAMUA6, $14.59. |
| Heat gun | SEEKONE 350 W mini, two temperatures | https://www.amazon.com/dp/B08VFY8THD | $17.99 | Use the low setting for heat shrink near silicone strip. Budget single-temperature: QWORK 300 W, https://www.amazon.com/dp/B09NDCCW29, $8.97. |

**Added 26 October 2026, perfboard wire.** Stranded silicone wire frays when stripped long and will not hold its shape on a board; it stays for the garment. For the shifter board rails and jumpers use solid core.

| Item | Pick | Link | Price | Notes |
|---|---|---|---|---|
| Bare tinned bus wire, 22 AWG, 100 ft | Jameco Valuepro 3818-100 | https://www.amazon.com/dp/B00B8866BA | $11.10 | Single strand, tinned, no insulation. For rails. Sold by Amazon. |
| Solid-core hookup wire kit, 22 AWG, 6 colors x 30 ft | TUOFENG 22 AWG Solid Wire Kit | https://www.amazon.com/dp/B07TX6BX47 | $16.19 | Listing states 1 strand, tinned copper, PVC. For jumpers. Alternative with fewer reviews: TMH solid 25 ft kit, https://www.amazon.com/dp/B0G43VFRM4, $14.99; select the **Solid** variant, the listing also sells stranded. |

**Added 1 November 2026, board holder.** The perfboard would not sit still on the bench. Checked on Amazon 1 November 2026.

| Item | Pick | Link | Price | Notes |
|---|---|---|---|---|
| PCB vise, recommended | Stickvise PCB Vise, standard nylon jaws | https://www.amazon.com/dp/B01LJO45W4 | $37.50 | Spring-loaded jaws hold the board flat at desk height; lift, flip, drop back in to work the other side. Holds the 5 x 7 cm board and an LED strip end alike. 4.7 stars, 189 reviews. Optional PTFE high-temperature jaws, https://www.amazon.com/dp/B01N5EDO1A, $18, not needed. |
| Alternative, tilting vise | PanaVise 201 Jr | https://www.amazon.com/dp/B00COSXIBE | $47.07 | The classic hobby vise: grooved jaws take a board edge, head tilts and rotates so the back comes up without unclamping. Heavier and taller than the Stickvise; better if you expect to hold odd shapes, worse for flat perfboard work. |
| Helping hands, for the cape joints later | QuadHands Workbench, 2 magnetic arms | https://www.amazon.com/dp/B07C1T3BMS | $29.95 | Weighted steel base, gooseneck arms with alligator clips. Holds a wire against a strip pad while both hands do iron and solder. Not a board holder. Made in USA, 4.7 stars, 454 reviews. 4-arm version, https://www.amazon.com/dp/B01MTWLF2Q, $49.95. |

**Budget route, about $110:** WEP 926LED V3 station kit (includes a tip cleaner), https://www.amazon.com/dp/B0BX2N258S, $28.99; NEIKO helping hands $9.99; MAIYUM solder $8.99; Chip Quik flux $11.89; TOWOT wick $7.99; KAIWEETS stripper $13.99; KATA cutters $6.99; AstroAI AM33D meter $14.59; QWORK heat gun $8.97.

## F. Dress form and mock-up supplies (added 24 October 2026)

For hanging the lit hardware on a stand-in cape before the designer starts, and for standing it behind the Kronos. Listings verified on Amazon.com on 24 October 2026. About $135 with the recommended picks.

**On the dress form.** Dial-adjustable male forms barely exist on Amazon: the proven one is the Dritz Mr. Tailor at $250, and the only in-budget one is a brand-new clone with a single two-star rating. For this job, a fixed-size foam form does everything needed: the cape hides the torso, and what matters is real shoulders, a neck, a pinnable surface, and a pole that puts the shoulder line at standing height. All the budget forms are about a 36 to 37 inch chest. If the designer wants to fit the garment on a form set to Mike's own measurements, that is the Dritz; otherwise they will drape on their own.

| Item | Pick | Link | Price | Notes |
|---|---|---|---|---|
| Male dress form, foam, pinnable, metal base | VINGLI Male Mannequin Torso, adjustable height, beige, metal base | https://www.amazon.com/dp/B0DWFN65D8 | $72.99 | Foam body under linen, so pins seat properly. Height 52 to 75 in, which puts the shoulder line anywhere from sitting to a tall standing adult. Flat metal base is the most stable footprint under a floor-length cape. Chest 36.5 in, waist 31.3 in, shoulder 16.2 in. Does not roll, but is light enough to carry in one hand. |
| Alternative, most reviews | Bonnlo Male Dress Form, wooden tripod, cotton cover | https://www.amazon.com/dp/B0CC8QXWN6 | $79.99 | Hard fiberglass core: pins only skim the cover. Fine for hanging a cape, worse for draping. |
| Alternative, dial-adjustable and designer-grade | Dritz Mr. Tailor Adjustable Dress Form, men's M to XL | https://www.amazon.com/dp/B08HBY4TVZ | $249.99 | Chest 37 to 45 in, 13 dials, foam-backed, pins at any angle. Only worth it if the designer will fit on it. Centered pole tops out at 62 in total height, so check it against shoulder-to-floor; the offset pole goes higher. |
| Mock cape | Rubie's full-length black velour cape with collar | https://www.amazon.com/dp/B001C929N4 | $16.13 | Matte velour, Prime. The collar is soft fabric, so slip a strip of cardboard inside it to make it stand. Listing does not give a numeric length. Explicit 54 in stand-collar alternative in smooth polyester, third-party shipping: https://www.amazon.com/dp/B001HMN1I8, $19.95 plus $6.95 shipping. |
| Black foam board for the bench, 20 x 30 in, 4-pack | BAZIC Black Foam Board 3/16 in | https://www.amazon.com/dp/B0GZLHP34N | $30.99 | Black bench surface to tape strips to while testing, so the LEDs are judged against matte black like the cape; a stiffener cut to fit inside the mock cape's floppy collar; and full-size flat layouts of zones to carry to the designer. Cheapest 20 x 30 multi-pack found. Smaller and cheaper, black through: EPHECH 20-pack 12 x 16.5 in, https://www.amazon.com/dp/B0H1VL55YH, $20.99. |
| Gaffer tape, 1 in x 55 yd, black matte cloth | ProTapes Pro Gaff | https://www.amazon.com/dp/B00DVCFFI6 | $15.86 | Genuine gaffer, comes off clean. Also useful on stage forever. |
| Black safety pins, 340 assorted | Huhuhero black safety pins | https://www.amazon.com/dp/B09YV5SNY3 | $8.39 | 160-pack option on the same page for $6.99. |
| Small black binder clips, 3/4 in, 60 | Ufmarine small binder clips | https://www.amazon.com/dp/B0CP4572WG | $4.99 | For clipping strips to the cape edges without piercing anything. |
| Black ball-head pins, 1.5 in, 500 | Sunenlyst glass ball head pins | https://www.amazon.com/dp/B0CBYK5NCG | $5.99 | For pinning into the foam form. T-pins also work and are the usual choice for foam. |

## G. Parts storage (added 29 October 2026)

Prices and stock checked on Amazon on 29 October 2026.

| Item | Product | Link | Price | Notes |
|---|---|---|---|---|
| Small parts case, recommended | DEWALT 20 Compartment Pro Organizer (DWST14830) | https://www.amazon.com/dp/B01MZZ6JU5 | $24.98 | 17.4 x 11 x 2.9 in, removable dividers, clear polycarbonate lid that presses on the dividers so parts cannot migrate between compartments when the case is tipped. For resistors, chips, sockets, headers, JST pigtails, fuses, small screws. Side latches stack it onto the DWST14825. |
| Bigger parts case | DEWALT 10 Compartment Deep Pro Organizer (DWST14825) | https://www.amazon.com/dp/B00AUVX394 | $25.97 | 18 x 15 x 5 in, 8 small and 2 large removable cups, IP53 seal, metal latches. For WAGOs, heat shrink, bullet strings, cable, the USB meter, the ESP32 in its bag. The one you already know. |
| Fishing-case alternative | Plano ProLatch 3700 StowAway, 2375002 | https://www.amazon.com/dp/B000E39T5A | $17.98 | 3 to 28 adjustable compartments, ProLatch snap latches, clear lid. Dividers lock into slots, so small parts stay put. Half-size version 23705-00, 4 to 16 compartments, https://www.amazon.com/dp/B000E39T5U, $11.21. Only 2 of the full-size left at time of checking. |

## Not on this list

- **Belt pouch.** Wait for the designer; they may make it to match the cape. For bench testing, any 6 x 4 inch belt pouch works.
- **Epaulet foam, collar interfacing, velvet, lining, trim.** Costume side, from the brief's page 5.
- **Tools** are now section E above.

## Order of operations

1. Order sections A, B and C together now; that is everything for the bench build and the first fitting.
2. The WIDI Master was ordered early, on 13 Nov 2026. It waits for phase 2; nothing in the hardware changes for it.
3. Order the 6N138 and DIN jack only if BLE lets you down in a venue.

## H. Printed shifter board (added 7 November 2026, ordered the same day)

The perfboard parts already on hand (DIP-14 socket, SN74AHCT125N, 220 ohm resistors, 1000 uF capacitor) all fit the printed board. The only new parts are the screw terminals; the board itself comes from the board house.

The six holes marked "ESP32-S3" are drawn for a 6-pin header. No pin header was ever bought for this project (this list said otherwise until 22 Nov 2026). As built, six jumper-wire halves are soldered straight into those holes, as on the perfboard. A 6-pin piece of 2.54 mm pin header fits if you would rather plug the leads on.

| Item | Part | Where | Notes |
|---|---|---|---|
| Printed board, 52 x 56 mm, 2 layer, set of 3 | `hardware/shifter-board/shifter-board-gerbers.zip` | https://oshpark.com | Ordered 7 Nov 2026. 9 to 12 days. |
| 3-pin screw terminal, 2.54 mm pitch, 20-pack | Hxchen 3-Pin 2.54mm 150V 6A | https://www.amazon.com/dp/B07WR8J1RP | Need 4. Delivered 9 Nov 2026. Generic KF128 style; the three pins go in the wire holes, the two peg holes on the footprint stay empty. Found by search on 7 Nov 2026, page not opened: check pitch 2.54 mm and price before ordering. |
| 2-pin screw terminal, 2.54 mm pitch, 20-pack | Hxchen 2-Pin 2.54mm 150V 6A | https://www.amazon.com/dp/B07WKL6MRM | Need 1, for the USB cable. Delivered 9 Nov 2026. Same caveat. Alternatives: QSU 3-pin 10-pack B081Y686TN, ANGSTROM 2-pin 20-pack B0FHGXH1JD, mixed 50-pack B0GMG6WHSN. The footprint is drawn for Phoenix MPT 0,5/3-2,54 (1725669) and MPT 0,5/2-2,54 (1725656), which also fit. |

## I. Noted during the perfboard-four build (8 November 2026)

| Item | Product | Link | Price | Notes |
|---|---|---|---|---|
| Small zip ties, 2.5 mm wide, 100 mm | any 100-pack | search Amazon for "2.5mm zip ties 100mm" | about $6 | For bundling cables in the enclosure. They do not pass through perfboard holes (1 mm); board strain relief is a twisted loop of solid wire through two holes. |
| Mini hot glue gun with clear sticks, 7 mm | any craft-store mini gun, low-temperature or dual | ordered 12 Nov 2026 | about $10 | Strain relief at every strip-end joint: a dab over the three pads, then heat shrink over it while soft. Not needed for bench tests; wanted before strips go into fabric. Added 12 Nov 2026. |
| ESP32-S3 DevKitC-1 N16R8, 3-pack with cables | QIQIAZI 3-pack (confirm brand on the page) | https://www.amazon.com/dp/B0H7HWWVBN | not verified | **Ordered 12 Nov 2026.** Spares and boards for later projects. Same pinout and WLED build (ESP32-S3 16MB opi) as the cape's controller. On arrival: check each one enumerates on USB and takes WLED before trusting it. |
