// The cape's lights: two lapels of 2 x 48 LEDs, plus the board's own LED.
//
// Wiring, the same as the WLED setup, so no lead has to move:
//     GPIO 4   lapel one (96 LEDs)
//     GPIO 5   lapel two (96 LEDs)
//     GPIO 6   output 3: bullet string (for now, a bench test pattern)
//     GPIO 7   output 4: bullet string (the same test pattern)
// The LED on the ESP32 board itself is not driven: all four LED outputs are in use.
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

enum class LinkState { Scanning, Connecting, Connected };

void capeBegin();

// A key was struck: a ripple spreads from the row for that pitch, as bright as
// the key was struck, and the row keeps a low glow while the key is held.
void capeNoteOn(uint8_t note, uint8_t velocity);
void capeNoteOff(uint8_t note);
void capeAllNotesOff();

// The sustain pedal moved (controller 64, 0-127). While it is down, released
// keys keep their glow; pressing further brightens them; lifting lets them go.
void capePedal(uint8_t value);

// Pitch bend moved (-8192 to 8191, 0 at rest). It turns the colour of
// everything that is lit: right towards aqua, left towards purple.
void capeBend(int bend);

// Draws one frame and sends it to the LEDs. Call often; it paces itself.
void capeShow(LinkState state);

// All lights out, now.
void capeBlank();
