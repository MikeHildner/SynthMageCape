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

// Presets. A preset is one complete look: what every light does when you
// play. Button two steps to the next one, and the cape remembers which one
// was in use when it was switched off. The list is in Cape.cpp (PRESETS).
//
//     1  "Roll With The Changes - Test"
//            The look built up to 21 Nov 2026, kept as it is for testing and
//            comparison. Named after the Kronos program it was tried with:
//            there, keys from C7 up play an organ (piano below), and on the
//            cape those keys do not ripple. While one is held, the lapels
//            carry a soft travelling wash of its colour.
//     2  "Default"
//            Mike's first own preset, being built. The lapels as in
//            preset 1, but without the organ keys: every key ripples.
//            On the strings every key sends a dot and every dot makes
//            the full trip: it starts as bright as the key was struck
//            and is still lit when it runs off the top of the outer
//            line. Mike plays standing behind the keyboard, which hides
//            his middle: the lapels show above it and the strings below
//            it, so every key should show in both places.
//            The collar is dark, except while a held key is pressed
//            down harder (aftertouch; see capeChannelPressure).
//            The shoulders come alive once four or more keys have been
//            held for half a second, and stay so while they are held:
//            every light on, in one colour picked at random each time.
//            With fewer they fade out and show the wavefronts as in
//            preset 1.
//            Joystick up (vibrato) makes the lapels, the strings and
//            the shoulders sparkle, more the further it is pushed. Not
//            the collar.
//
//     3  "Purple Rain"
//            Rain, in purples, and nothing else. Drops fall down the
//            four columns of the lapels and on down the strings; the
//            shoulders flash where drops land; the collar is the
//            cloud, a deep violet that gathers while notes sound.
//            A faint drizzle with nothing played, steady rain while
//            notes sound, a drop for every key struck, a downpour
//            with the joystick up. See "the Purple Rain preset" in
//            Cape.cpp.
//
// No preset sets the brightness.
uint8_t capePresetCount();
uint8_t capePresetNumber();  // 1 for the first
const char* capePresetName();

// Steps to the next preset, round to the first after the last. For a second
// the lapels show as many white bars as the new preset's number.
void capeNextPreset();

enum class LinkState { Scanning, Connecting, Connected };

void capeBegin();

// A key was struck: a ripple spreads from the row for that pitch, as bright as
// the key was struck, and the row keeps a low glow while the key is held.
// (An organ key starts a wash instead; see the presets above.)
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
// six times a second, wider the further the joystick is pushed. In the
// Default preset it also makes everything but the collar sparkle.
void capeModulation(uint8_t value);

// Aftertouch: how hard the held keys are being pressed down after they were
// struck (Channel Pressure, 0-127, 0 when the pressure is let go; one value
// for the whole keyboard). In a preset whose collar waits for it, the collar
// is dark until the pressure reaches COLLAR_PRESSURE_ON, then comes up
// quickly and shows the notes sounding; when the pressure is back at 0 it
// falls away, more slowly. Other presets take no notice.
void capeChannelPressure(uint8_t value);

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
