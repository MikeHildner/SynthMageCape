#include "Cape.h"

#include <Arduino.h>
#include <FastLED.h>

namespace {

const uint8_t LAPEL_ONE_PIN = 4;
const uint8_t LAPEL_TWO_PIN = 5;
const uint8_t ONBOARD_LED_PIN = 48;

const uint8_t ROWS = 48;
const uint8_t LEDS_PER_LAPEL = 2 * ROWS;

CRGB lapelOne[LEDS_PER_LAPEL];
CRGB lapelTwo[LEDS_PER_LAPEL];
CRGB onboard[1];

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

// level 0..1 to the warm white the note light used in step 3.
CRGB warm(float level) {
  if (level > 1) level = 1;
  return CRGB((uint8_t)(255 * level), (uint8_t)(150 * level), (uint8_t)(50 * level));
}

void drawNotes(uint32_t now) {
  float level[ROWS] = {0};

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
    const uint8_t row = (uint8_t)(rowOfNote(note) + 0.5f);
    const float glow = HELD_GLOW * heldVelocity[note] / 127.0f;
    if (level[row] < glow) level[row] = glow;
  }

  for (uint8_t row = 0; row < ROWS; row++) setRow(row, warm(level[row]));
}

}  // namespace

void capeBegin() {
  FastLED.addLeds<WS2812B, LAPEL_ONE_PIN, GRB>(lapelOne, LEDS_PER_LAPEL);
  FastLED.addLeds<WS2812B, LAPEL_TWO_PIN, GRB>(lapelTwo, LEDS_PER_LAPEL);
  FastLED.addLeds<WS2812B, ONBOARD_LED_PIN, GRB>(onboard, 1);
  // The two limits. Everything drawn goes through both.
  FastLED.setBrightness(MAX_BRIGHTNESS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, MAX_MILLIAMPS);
  FastLED.clear(true);
}

void capeNoteOn(uint8_t note, uint8_t velocity) {
  note &= 0x7F;
  heldVelocity[note] = velocity;

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

void capeNoteOff(uint8_t note) { heldVelocity[note & 0x7F] = 0; }

void capeBlank() { FastLED.clear(true); }

void capeAllNotesOff() {
  memset(heldVelocity, 0, sizeof(heldVelocity));
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
    // Not connected: the hem row and the board's LED blink blue while
    // searching and hold steady while connecting. Everything else is dark.
    const bool lit = state == LinkState::Connecting || (now / 500) % 2;
    fill_solid(lapelOne, LEDS_PER_LAPEL, CRGB::Black);
    fill_solid(lapelTwo, LEDS_PER_LAPEL, CRGB::Black);
    setRow(0, lit ? CRGB(0, 0, 120) : CRGB::Black);
    onboard[0] = lit ? CRGB(0, 0, 120) : CRGB::Black;
  } else {
    drawNotes(now);
    onboard[0] = CRGB(0, 40, 0);  // faint green: connected

    // Just connected: one soft green wavefront climbs the lapels, so the
    // wearer can see the link is up without looking at the box.
    const uint32_t sinceConnect = now - connectedAtMs;
    const uint32_t SWEEP_MS = 600;
    if (sinceConnect < SWEEP_MS) {
      const uint8_t row = (uint8_t)(sinceConnect * (ROWS - 1) / SWEEP_MS);
      setRow(row, CRGB(0, 120, 0));
    }
  }

  FastLED.show();
}
