#include "Cape.h"

#include <Arduino.h>
#include <FastLED.h>

namespace {

const uint8_t LAPEL_ONE_PIN = 4;
const uint8_t LAPEL_TWO_PIN = 5;
// The LED on the ESP32 board itself (GPIO 48) is no longer driven. The chip
// has four LED outputs, and the cape needs all four: two lapels, two strings.
// The hem row of the lapels shows the link state instead.

const uint8_t ROWS = 48;
const uint8_t LEDS_PER_LAPEL = 2 * ROWS;

CRGB lapelOne[LEDS_PER_LAPEL];
CRGB lapelTwo[LEDS_PER_LAPEL];

// ---- bullet string bench test (temporary)
//
// One whole, uncut string of 50, to learn three things before anything is
// cut: that every pixel works, which way the data runs, and the colour order.
// The same picture goes out on outputs 3 and 4, so either free pigtail on the
// box will do. It is always the same, whatever is being played:
//     pixel 1 red, pixel 2 green, pixel 3 blue   (if not, the colour order is wrong)
//     the last pixel dim white                   (so the count is right)
//     one dim white dot walking from pixel 4 towards the far end
const uint8_t STRING_ONE_PIN = 6;  // output 3 on the shifter board
const uint8_t STRING_TWO_PIN = 7;  // output 4
const uint8_t STRING_TEST_PIXELS = 50;
CRGB stringOne[STRING_TEST_PIXELS];

void drawStringTest(uint32_t now) {
  fill_solid(stringOne, STRING_TEST_PIXELS, CRGB::Black);
  stringOne[0] = CRGB(255, 0, 0);
  stringOne[1] = CRGB(0, 255, 0);
  stringOne[2] = CRGB(0, 0, 255);
  stringOne[STRING_TEST_PIXELS - 1] = CRGB(90, 90, 90);
  const uint8_t walkers = STRING_TEST_PIXELS - 4;  // pixels 4 to 49
  stringOne[3 + (now / 120) % walkers] = CRGB(140, 140, 140);
}

const uint32_t FRAME_MS = 10;  // 100 frames a second

// ---- ripples

// How a struck key looks: two wavefronts leave the key's row, one towards the
// collar and one towards the hem, fading as they go.
const float RIPPLE_LIFE_MS = 700;        // how long a ripple lasts
const float RIPPLE_ROWS_PER_MS = 0.045f; // how fast the wavefronts travel
const float RIPPLE_WIDTH_ROWS = 2.5f;    // softness of a wavefront
const float HELD_GLOW = 0.30f;           // a held key glows at this fraction of its strike

struct Ripple {
  bool active;
  uint8_t note;    // decides the colour
  float row;
  float strength;  // 0..1, from velocity
  uint32_t startMs;
};
Ripple ripples[16];

uint8_t heldVelocity[128];  // 0 = key up

// ---- the sustain pedal
//
// Like a piano's dampers: a key released while the pedal is down keeps its
// glow. How far down the pedal is sets how bright those glows are, they dim
// slowly while the pedal stays down, and they fall away when it comes up.
const uint8_t PEDAL_DOWN_AT = 64;         // controller 64 at or above this counts as down
const float PEDAL_HOLD_FADE_MS = 4000;    // pedal down: glows fall to about a third in this time
const float PEDAL_RELEASE_FADE_MS = 250;  // pedal up: glows fall away this fast

float sustainLevel[128];  // 0..1 glow of a note the pedal is holding; 0 = none
bool pedalDown = false;
float pedalDepth = 0;     // 0.5 (just down) to 1 (fully down)

uint32_t connectedAtMs = 0;
LinkState lastState = LinkState::Scanning;

bool isOrganNote(uint8_t note) { return note >= ORGAN_FROM_NOTE; }

float rowOfNote(uint8_t note) {
  if (note <= LOWEST_NOTE) return 0;
  if (note >= HIGHEST_NOTE) return ROWS - 1;
  return (float)(note - LOWEST_NOTE) * (ROWS - 1) / (HIGHEST_NOTE - LOWEST_NOTE);
}

// Lights one row (both columns) on both lapels.
void setRow(uint8_t row, const CRGB& colour) {
  lapelOne[row] = colour;
  lapelOne[LEDS_PER_LAPEL - 1 - row] = colour;
  lapelTwo[row] = colour;
  lapelTwo[LEDS_PER_LAPEL - 1 - row] = colour;
}

// ---- colour, and pitch bend
//
// Hue is a position on FastLED's colour wheel, 0-255: 0 red, 32 orange,
// 64 yellow, 96 green, 128 aqua, 160 blue, 192 purple, 224 pink.
//
// Look::Warm: every note is the same warm white.
// Look::NoteNames: the twelve note names are spread evenly round the wheel,
// starting with C at NOTE_HUE_OF_C, in full colour. A C is the same colour in
// every octave; a scale walks round the wheel; a chord shows its notes as
// separate colours.
//
// Pitch bend turns the wheel for everything that is lit, by up to
// BEND_HUE_RANGE either way. The joystick springs back, and so does the colour.
const uint8_t BASE_HUE = 36;          // Warm look: warm orange...
const uint8_t BASE_SATURATION = 170;  // ...with enough white in it to read as warm white
const float COLOUR_GAIN = 1.42f;      // Warm look: brings that colour up to full scale
const uint8_t NOTE_HUE_OF_C = 0;      // NoteNames look: C is red
const float BEND_HUE_RANGE = 96;      // how far full bend turns the wheel, either way

uint8_t hueShift = 0;  // from pitch bend; added to every hue, wrapping round the wheel

// A colour in red, green and blue, 0-255 each, kept as fractions while several
// lights are being mixed. It may run past 255 on the way; toLeds() limits it.
struct Colour {
  float r, g, b;
};

// The colour of a note at full level.
Colour colourOfNote(uint8_t note) {
  uint8_t hue = BASE_HUE;
  uint8_t saturation = BASE_SATURATION;
  float gain = COLOUR_GAIN;
  if (LOOK == Look::NoteNames) {
    hue = (uint8_t)(NOTE_HUE_OF_C + (note % 12) * 256 / 12);
    saturation = 255;
    gain = 1;
  }
  const CRGB full = CHSV((uint8_t)(hue + hueShift), saturation, 255);
  return {full.r * gain, full.g * gain, full.b * gain};
}

// Adds `amount` (0..1) of a colour to what is there.
void addColour(Colour& to, const Colour& colour, float amount) {
  to.r += colour.r * amount;
  to.g += colour.g * amount;
  to.b += colour.b * amount;
}

// Raises what is there to at least `amount` of a colour, channel by channel.
void raiseColour(Colour& to, const Colour& colour, float amount) {
  to.r = fmaxf(to.r, colour.r * amount);
  to.g = fmaxf(to.g, colour.g * amount);
  to.b = fmaxf(to.b, colour.b * amount);
}

CRGB toLeds(const Colour& colour) {
  return CRGB((uint8_t)fminf(255.0f, colour.r), (uint8_t)fminf(255.0f, colour.g),
              (uint8_t)fminf(255.0f, colour.b));
}

// ---- the organ keys
//
// Keys from ORGAN_FROM_NOTE up play an organ, which sounds for as long as the
// key is down and does not die away. So these keys do not ripple. While one is
// held, both lapels carry a soft wash of its colour, with slow swells
// travelling up them, like the turning speaker of an organ. Several organ keys
// mix their colours. The sustain pedal does not hold the wash.
const float ORGAN_WASH_LEVEL = 0.22f;   // the wash at its brightest, 0..1 of the ceiling
const float ORGAN_SWELL_DEPTH = 0.65f;  // how far the wash dips between swells, 0 = flat
const float ORGAN_SWELL_ROWS = 16;      // length of one swell along the lapel
const float ORGAN_SWELL_MS = 900;       // time a swell takes to travel its own length
const float ORGAN_ATTACK_MS = 60;       // the wash comes up this fast
const float ORGAN_RELEASE_MS = 300;     // and falls away this fast

float organLevel[128];  // 0..1, how far each organ key's wash has come up

// ---- sparkle
//
// The bigger the chord, the more the lapels shimmer: single LEDs anywhere on
// either lapel flash briefly. Counted are the keys held down plus the notes
// the pedal is holding. In the Warm look a sparkle is nearly white; in the
// NoteNames look each sparkle takes a colour of its own.
const uint8_t SPARKLE_FROM_NOTES = 3;      // no sparkle below this many notes
const uint8_t SPARKLE_FULL_AT_NOTES = 8;   // this many or more gives the full rate
const float SPARKLES_PER_SECOND = 30;      // the full rate, across both lapels
const float SPARKLE_FADE_MS = 150;         // a sparkle falls to about a third in this time
const uint8_t SPARKLE_SATURATION = 70;         // Warm look: low = nearly white
const uint8_t SPARKLE_SATURATION_COLOUR = 200; // NoteNames look: coloured, a little pale

float sparkle[2][LEDS_PER_LAPEL];       // 0..1, per LED, per lapel
uint8_t sparkleHue[2][LEDS_PER_LAPEL];  // NoteNames look: each sparkle's own hue

void drawSparkle(uint8_t notesSounding, float elapsedMs) {
  // New sparkles for this frame. The fractional part becomes a probability.
  float rate = 0;
  if (notesSounding >= SPARKLE_FROM_NOTES) {
    const float span = SPARKLE_FULL_AT_NOTES - SPARKLE_FROM_NOTES + 1;
    rate = SPARKLES_PER_SECOND * min(1.0f, (notesSounding - SPARKLE_FROM_NOTES + 1) / span);
  }
  float due = rate * elapsedMs / 1000.0f;
  while (due > 0) {
    if (due >= 1 || random8() < (uint8_t)(due * 255)) {
      const uint8_t lapel = random8(2);
      const uint8_t led = random8(LEDS_PER_LAPEL);
      sparkle[lapel][led] = 0.5f + random8() / 510.0f;  // 0.5 to 1
      sparkleHue[lapel][led] = random8();
    }
    due -= 1;
  }

  // Fade the old ones and add them on top of what is already drawn.
  const float keep = expf(-elapsedMs / SPARKLE_FADE_MS);
  CRGB* const lapels[2] = {lapelOne, lapelTwo};
  for (uint8_t lapel = 0; lapel < 2; lapel++) {
    for (uint8_t i = 0; i < LEDS_PER_LAPEL; i++) {
      float& level = sparkle[lapel][i];
      if (level == 0) continue;
      level *= keep;
      if (level < 0.02f) {
        level = 0;
        continue;
      }
      const uint8_t value = (uint8_t)(255 * level);
      if (LOOK == Look::NoteNames) {
        lapels[lapel][i] += CHSV((uint8_t)(sparkleHue[lapel][i] + hueShift), SPARKLE_SATURATION_COLOUR, value);
      } else {
        lapels[lapel][i] += CHSV((uint8_t)(BASE_HUE + hueShift), SPARKLE_SATURATION, value);
      }
    }
  }
}

// Adds the organ keys' wash to every row.
void addOrganWash(Colour rows[], uint32_t now, float elapsedMs) {
  Colour mix = {0, 0, 0};
  float total = 0;
  for (uint8_t note = ORGAN_FROM_NOTE; note < 128; note++) {
    float& level = organLevel[note];
    const float target = heldVelocity[note] > 0 ? 1.0f : 0.0f;
    if (level == target) {
      if (level == 0) continue;
    } else {
      const float time = target > level ? ORGAN_ATTACK_MS : ORGAN_RELEASE_MS;
      level += (target - level) * (1.0f - expf(-elapsedMs / time));
      if (target == 0 && level < 0.01f) {
        level = 0;
        continue;
      }
    }
    addColour(mix, colourOfNote(note), level);
    total += level;
  }
  if (total == 0) return;

  // The mix is the average colour of the organ keys. More keys do not make
  // the wash brighter: one key already brings it to its level.
  const float amount = ORGAN_WASH_LEVEL * fminf(1.0f, total) / total;
  const float phase = (float)(now % (uint32_t)ORGAN_SWELL_MS) / ORGAN_SWELL_MS;
  for (uint8_t row = 0; row < ROWS; row++) {
    const float swell = 0.5f + 0.5f * sinf(TWO_PI * (row / ORGAN_SWELL_ROWS - phase));
    addColour(rows[row], mix, amount * (1.0f - ORGAN_SWELL_DEPTH * (1.0f - swell)));
  }
}

void drawNotes(uint32_t now) {
  static uint32_t lastDrawMs = now;
  const float elapsed = (float)(now - lastDrawMs);
  lastDrawMs = now;

  Colour rows[ROWS] = {};
  float level[ROWS] = {0};
  uint8_t notesSounding = 0;

  for (Ripple& ripple : ripples) {
    if (!ripple.active) continue;
    const float age = (float)(now - ripple.startMs);
    if (age >= RIPPLE_LIFE_MS) {
      ripple.active = false;
      continue;
    }
    const float remaining = 1.0f - age / RIPPLE_LIFE_MS;
    const float fade = remaining * remaining;
    const float radius = age * RIPPLE_ROWS_PER_MS;
    const Colour colour = colourOfNote(ripple.note);
    for (uint8_t row = 0; row < ROWS; row++) {
      // Distance from this row to the nearer of the two wavefronts.
      const float distance = fabsf(fabsf((float)row - ripple.row) - radius);
      if (distance < RIPPLE_WIDTH_ROWS) {
        const float amount = ripple.strength * fade * (1.0f - distance / RIPPLE_WIDTH_ROWS);
        addColour(rows[row], colour, amount);
        level[row] += amount;
      }
    }
  }
  // Where ripples pile up past full level, bring the row back to full level
  // and keep the mix of colours.
  for (uint8_t row = 0; row < ROWS; row++) {
    if (level[row] > 1) {
      rows[row].r /= level[row];
      rows[row].g /= level[row];
      rows[row].b /= level[row];
    }
  }

  // Keys being held: a low glow on the key's row. (Organ keys wash instead.)
  for (uint8_t note = 0; note < 128; note++) {
    if (heldVelocity[note] == 0) continue;
    notesSounding++;
    if (isOrganNote(note)) continue;
    const uint8_t row = (uint8_t)(rowOfNote(note) + 0.5f);
    raiseColour(rows[row], colourOfNote(note), HELD_GLOW * heldVelocity[note] / 127.0f);
  }

  // Notes the pedal is holding. They fade slowly while it is down and quickly
  // once it is up.
  const float keep = expf(-elapsed / (pedalDown ? PEDAL_HOLD_FADE_MS : PEDAL_RELEASE_FADE_MS));
  for (uint8_t note = 0; note < 128; note++) {
    if (sustainLevel[note] == 0) continue;
    sustainLevel[note] *= keep;
    if (sustainLevel[note] < 0.01f) {
      sustainLevel[note] = 0;
      continue;
    }
    if (pedalDown) notesSounding++;
    const uint8_t row = (uint8_t)(rowOfNote(note) + 0.5f);
    raiseColour(rows[row], colourOfNote(note), sustainLevel[note] * (pedalDown ? pedalDepth : 1.0f));
  }

  addOrganWash(rows, now, elapsed);

  for (uint8_t row = 0; row < ROWS; row++) setRow(row, toLeds(rows[row]));

  drawSparkle(notesSounding, elapsed);
}

}  // namespace

void capeBegin() {
  FastLED.addLeds<WS2812B, LAPEL_ONE_PIN, GRB>(lapelOne, LEDS_PER_LAPEL);
  FastLED.addLeds<WS2812B, LAPEL_TWO_PIN, GRB>(lapelTwo, LEDS_PER_LAPEL);
  // Bullet pixels use the WS2811 chip, and most strings take colours as
  // red, green, blue (the strips take green, red, blue). The test shows if not.
  FastLED.addLeds<WS2811, STRING_ONE_PIN, RGB>(stringOne, STRING_TEST_PIXELS);
  FastLED.addLeds<WS2811, STRING_TWO_PIN, RGB>(stringOne, STRING_TEST_PIXELS);  // same picture
  // The two limits. Everything drawn goes through both.
  FastLED.setBrightness(MAX_BRIGHTNESS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, MAX_MILLIAMPS);
  FastLED.clear(true);
}

void capeNoteOn(uint8_t note, uint8_t velocity) {
  note &= 0x7F;
  heldVelocity[note] = velocity;
  sustainLevel[note] = 0;  // struck again: the key's own glow takes over
  if (isOrganNote(note)) return;  // organ keys wash, they do not ripple

  // Reuse a finished ripple, or failing that the oldest one.
  Ripple* slot = &ripples[0];
  for (Ripple& ripple : ripples) {
    if (!ripple.active) {
      slot = &ripple;
      break;
    }
    if (ripple.startMs < slot->startMs) slot = &ripple;
  }
  slot->active = true;
  slot->note = note;
  slot->row = rowOfNote(note);
  slot->strength = velocity / 127.0f;
  slot->startMs = millis();
}

void capeNoteOff(uint8_t note) {
  note &= 0x7F;
  // Key up with the pedal down: the pedal takes over the glow.
  if (pedalDown && heldVelocity[note] > 0 && !isOrganNote(note)) {
    sustainLevel[note] = HELD_GLOW * heldVelocity[note] / 127.0f;
  }
  heldVelocity[note] = 0;
}

void capePedal(uint8_t value) {
  const bool down = value >= PEDAL_DOWN_AT;
  if (down) {
    // Half-pedalling: 64 gives half brightness, 127 gives full.
    pedalDepth = 0.5f + 0.5f * (float)(value - PEDAL_DOWN_AT) / (127 - PEDAL_DOWN_AT);
  } else if (pedalDown) {
    // Coming up: the glows start from the brightness they were showing.
    for (float& glow : sustainLevel) glow *= pedalDepth;
  }
  pedalDown = down;
}

void capeBend(int bend) {
  // bend is -8192..8191. Turning the result into a uint8_t wraps a turn to
  // the left (negative) round the colour wheel.
  hueShift = (uint8_t)(int)lroundf(bend * BEND_HUE_RANGE / 8192.0f);
}

void capeBlank() { FastLED.clear(true); }

void capeAllNotesOff() {
  hueShift = 0;
  memset(heldVelocity, 0, sizeof(heldVelocity));
  memset(sustainLevel, 0, sizeof(sustainLevel));
  memset(organLevel, 0, sizeof(organLevel));
  memset(sparkle, 0, sizeof(sparkle));
  pedalDown = false;
  for (Ripple& ripple : ripples) ripple.active = false;
}

void capeShow(LinkState state) {
  static uint32_t lastFrameMs = 0;
  const uint32_t now = millis();
  if (now - lastFrameMs < FRAME_MS) return;
  lastFrameMs = now;

  if (state == LinkState::Connected && lastState != LinkState::Connected) connectedAtMs = now;
  lastState = state;

  if (state != LinkState::Connected) {
    // Not connected: the hem row blinks blue while searching and holds
    // steady while connecting. Everything else is dark.
    const bool lit = state == LinkState::Connecting || (now / 500) % 2;
    fill_solid(lapelOne, LEDS_PER_LAPEL, CRGB::Black);
    fill_solid(lapelTwo, LEDS_PER_LAPEL, CRGB::Black);
    setRow(0, lit ? CRGB(0, 0, 120) : CRGB::Black);
  } else {
    drawNotes(now);

    // Just connected: one soft green wavefront climbs the lapels, so the
    // wearer can see the link is up without looking at the box.
    const uint32_t sinceConnect = now - connectedAtMs;
    const uint32_t SWEEP_MS = 600;
    if (sinceConnect < SWEEP_MS) {
      const uint8_t row = (uint8_t)(sinceConnect * (ROWS - 1) / SWEEP_MS);
      setRow(row, CRGB(0, 120, 0));
    }
  }

  drawStringTest(now);
  FastLED.show();
}
