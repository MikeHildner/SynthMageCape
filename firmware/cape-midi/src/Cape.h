// The cape's lights: two lapels of 2 x 48 LEDs, a string of 50 small lights
// after each lapel, the collar strip, and two shoulders of 2 x 12 LEDs.
//
// Wiring:
//     GPIO 4   output 1: lapel one (96 LEDs), then its string (50 lights)
//     GPIO 5   output 2: lapel two (96 LEDs), then its string (50 lights)
//     GPIO 6   output 3: the collar strip (on the bench, the whole 2 m)
//     GPIO 7   output 4: shoulder one (24 LEDs), then shoulder two (24 LEDs)
// The LED on the ESP32 board itself is not driven; the chip's four LED
// outputs are kept for the cape.
//
// Each lapel is one chain of 96: strip one runs up from the plug at the hem to
// the collar (LEDs 0-47), a U-turn, then strip two runs back down (48-95).
// So a "row" (0 at the hem, 47 at the collar) is two LEDs: `row` and `95 - row`.
#pragma once

#include <stdint.h>

// Brightness ceiling for everything this firmware shows, 0-255.
// The cape is for a dark stage: less is more. Raise it only on purpose.
const uint8_t MAX_BRIGHTNESS = 32;

// The same electrical limit WLED was given: the whole cape may draw this much.
const uint16_t MAX_MILLIAMPS = 1500;

// The keys that map onto the lapel, hem to collar. 21-108 is an 88-key piano
// (A0 to C8). Notes outside the range light the nearest end.
const uint8_t LOWEST_NOTE = 21;
const uint8_t HIGHEST_NOTE = 108;

// Which colours the notes get.
//     Warm       every note the same warm white (the first look, 17 Nov)
//     NoteNames  each note name has its own colour, the same in every octave:
//                C red, then round the colour wheel a step per semitone
//                (D orange-yellow, E yellow-green, F green, G blue, A purple, B pink)
// Neither look changes the brightness ceiling above.
enum class Look { Warm, NoteNames };
const Look LOOK = Look::NoteNames;

// Keys from this note up play an organ on the Kronos program in use (piano
// below). Organ keys do not ripple: while one is held, the lapels carry a soft
// travelling wash of its colour. 96 is C7 (60 is C4). Set to 128 for no organ keys.
const uint8_t ORGAN_FROM_NOTE = 96;

enum class LinkState { Scanning, Connecting, Connected };

void capeBegin();

// A key was struck: a ripple spreads from the row for that pitch, as bright as
// the key was struck, and the row keeps a low glow while the key is held.
// (An organ key starts a wash instead; see ORGAN_FROM_NOTE.)
void capeNoteOn(uint8_t note, uint8_t velocity);
void capeNoteOff(uint8_t note);
void capeAllNotesOff();

// The sustain pedal moved (controller 64, 0-127). While it is down, released
// keys keep their glow; pressing further brightens them; lifting lets them go.
void capePedal(uint8_t value);

// Pitch bend moved (-8192 to 8191, 0 at rest). It turns the colour of
// everything that is lit, by up to three eighths of a turn either way.
void capeBend(int bend);

// The mod wheel moved (controller 1, 0-127; joystick up on the Kronos, which
// adds vibrato). The colour of everything that is lit swings to and fro,
// six times a second, wider the further the joystick is pushed.
void capeModulation(uint8_t value);

// Draws one frame and sends it to the LEDs. Call often; it paces itself.
void capeShow(LinkState state);

// All lights out, now.
void capeBlank();

// How many frames went out to the LEDs in the last full second. For the
// status page: the collar's smooth fade needs this near 200.
uint16_t capeFramesPerSecond();

// The longest time between two frames, in milliseconds, since this was last
// asked. A frame normally takes about 10; a much larger number means
// something held the lights up. For the status page.
uint16_t capeLongestGapMs();
