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
// At rest everything is a warm white. Pitch bend turns the wheel for everything
// that is lit: fully right reaches aqua by way of yellow and green, fully left
// reaches purple by way of red and pink. The joystick springs back, and so
// does the colour.
const uint8_t BASE_HUE = 36;          // warm orange...
const uint8_t BASE_SATURATION = 170;  // ...with enough white in it to read as warm white
const float BEND_HUE_RANGE = 96;      // how far full bend turns the wheel, either way
const float COLOUR_GAIN = 1.42f;      // brings the resting colour up to full scale

uint8_t hueNow = BASE_HUE;

// level 0..1 to a colour at the current hue.
CRGB colourAt(float level) {
  if (level <= 0) return CRGB::Black;
  if (level > 1) level = 1;
  const CRGB full = CHSV(hueNow, BASE_SATURATION, 255);
  const float gain = level * COLOUR_GAIN;
  return CRGB((uint8_t)min(255.0f, full.r * gain), (uint8_t)min(255.0f, full.g * gain),
              (uint8_t)min(255.0f, full.b * gain));
}

// ---- sparkle
//
// The bigger the chord, the more the lapels shimmer: single LEDs anywhere on
// either lapel flash briefly, paler than the note colour. Counted are the keys
// held down plus the notes the pedal is holding.
const uint8_t SPARKLE_FROM_NOTES = 3;      // no sparkle below this many notes
const uint8_t SPARKLE_FULL_AT_NOTES = 8;   // this many or more gives the full rate
const float SPARKLES_PER_SECOND = 30;      // the full rate, across both lapels
const float SPARKLE_FADE_MS = 150;         // a sparkle falls to about a third in this time
const uint8_t SPARKLE_SATURATION = 70;     // low = nearly white

float sparkle[2][LEDS_PER_LAPEL];  // 0..1, per LED, per lapel

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
      sparkle[random8(2)][random8(LEDS_PER_LAPEL)] = 0.5f + random8() / 510.0f;  // 0.5 to 1
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
      lapels[lapel][i] += CHSV(hueNow, SPARKLE_SATURATION, (uint8_t)(255 * level));
    }
  }
}

void drawNotes(uint32_t now) {
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
    for (uint8_t row = 0; row < ROWS; row++) {
      // Distance from this row to the nearer of the two wavefronts.
      const float distance = fabsf(fabsf((float)row - ripple.row) - radius);
      if (distance < RIPPLE_WIDTH_ROWS) {
        level[row] += ripple.strength * fade * (1.0f - distance / RIPPLE_WIDTH_ROWS);
      }
    }
  }

  for (uint8_t note = 0; note < 128; note++) {
    if (heldVelocity[note] == 0) continue;
    notesSounding++;
    const uint8_t row = (uint8_t)(rowOfNote(note) + 0.5f);
    const float glow = HELD_GLOW * heldVelocity[note] / 127.0f;
    if (level[row] < glow) level[row] = glow;
  }

  // Notes the pedal is holding. They fade slowly while it is down and quickly
  // once it is up.
  static uint32_t lastDrawMs = now;
  const float elapsed = (float)(now - lastDrawMs);
  lastDrawMs = now;
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
    const float glow = sustainLevel[note] * (pedalDown ? pedalDepth : 1.0f);
    if (level[row] < glow) level[row] = glow;
  }

  for (uint8_t row = 0; row < ROWS; row++) setRow(row, colourAt(level[row]));

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
  slot->row = rowOfNote(note);
  slot->strength = velocity / 127.0f;
  slot->startMs = millis();
}

void capeNoteOff(uint8_t note) {
  note &= 0x7F;
  // Key up with the pedal down: the pedal takes over the glow.
  if (pedalDown && heldVelocity[note] > 0) {
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
  // bend is -8192..8191; uint8_t arithmetic wraps round the colour wheel.
  hueNow = (uint8_t)(BASE_HUE + (int)lroundf(bend * BEND_HUE_RANGE / 8192.0f));
}

void capeBlank() { FastLED.clear(true); }

void capeAllNotesOff() {
  hueNow = BASE_HUE;
  memset(heldVelocity, 0, sizeof(heldVelocity));
  memset(sustainLevel, 0, sizeof(sustainLevel));
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
