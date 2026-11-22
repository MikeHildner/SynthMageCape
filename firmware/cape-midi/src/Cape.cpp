#include "Cape.h"

#include <Arduino.h>
#include <FastLED.h>
#include <Preferences.h>

namespace {

const uint8_t LAPEL_ONE_PIN = 4;
const uint8_t LAPEL_TWO_PIN = 5;
// The LED on the ESP32 board itself (GPIO 48) is not driven. The chip has
// four LED outputs, and the cape uses all four. Output 4 (GPIO 7) carries
// the shoulders (see drawShoulders).
// The hem row of the lapels shows the link state instead.

const uint8_t ROWS = 48;
const uint8_t LEDS_PER_LAPEL = 2 * ROWS;

// A fairy string of 50 lights is chained after each lapel: it plugs into the
// far end of the lapel's second strip, back down at the hem, and takes its
// data from the lapel's last LED (soldered and proven 20 Nov). So each lapel
// output carries 96 + 50 pixels. The string's input is the end with the two
// loose wires; its lights are 10 cm apart and take the same signal and
// colour order as the strips. The ruler that tested all this is in git
// history (drawRuler, commit 0ff377c and before).
const uint8_t STRAND_LIGHTS = 50;
const uint8_t LAPEL_CHAIN_PIXELS = LEDS_PER_LAPEL + STRAND_LIGHTS;

CRGB lapelOne[LAPEL_CHAIN_PIXELS];
CRGB lapelTwo[LAPEL_CHAIN_PIXELS];

// The least time between frames. Each pixel takes 30 millionths of a second
// to send, and the LED driver sends two strips at a time: the two lapels
// with their strings together (146 pixels, 4.4 ms), then the collar (128
// pixels, 4 ms). Measured on the status page: 125 frames a second with
// lapels and collar (19 Nov), 111 with the strings, 108 with 24 shoulder
// pixels on the fourth output and 102 with 48 (20 Nov). So the shoulders do
// cost time, though they are sent beside the collar; why is not yet looked
// into. The collar's smooth fade depends on this rate (see
// ditheredChannel), so anything that lengthens a frame is worth a second look.
const uint32_t FRAME_MS = 5;

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

// ---- presets (described in Cape.h)
struct Preset {
  const char* name;
  uint8_t organFromNote;  // keys from this note up are organ keys; 128 for none
  bool dotsFullTrip;      // the strings: every key sends a dot that makes the full trip (see "the strings")
  bool collarOnPressure;  // the collar is dark except under aftertouch (see "the collar")
  bool shouldersOnChord;  // the shoulders come alive while four or more keys are held (see "the shoulders")
  bool sparkleOnJoystick; // joystick up makes everything but the collar sparkle (see "sparkle from the joystick")
  bool rain;              // a look of its own, and the fields above do not apply (see "the Purple Rain preset")
};
const Preset PRESETS[] = {
    {"Roll With The Changes - Test", 96, false, false, false, false, false},  // 96 is C7 (60 is C4)
    {"Default", 128, true, true, true, true, false},
    {"Purple Rain", 128, false, false, false, false, true},
};
const uint8_t PRESET_COUNT = sizeof(PRESETS) / sizeof(PRESETS[0]);
uint8_t presetNow = 0;         // index into PRESETS
uint32_t presetChosenMs = 0;   // when button two last chose one; 0 for not yet
Preferences settings;          // the cape's small permanent store: survives power off

bool isOrganNote(uint8_t note) { return note >= PRESETS[presetNow].organFromNote; }

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

// ---- vibrato
//
// Joystick up sends controller 1 (the mod wheel), which on most programs adds
// vibrato: the pitch wobbles a little, several times a second. The lights do
// the same with colour. Everything that is lit swings to and fro round its
// own colour, VIBRATO_HZ times a second, and the further the joystick is
// pushed the wider the swing. Brightness is not touched.
const float VIBRATO_HUE_RANGE = 32;  // the swing either way at full joystick: an eighth of a turn
const float VIBRATO_HZ = 6;          // swings a second

float bendShift = 0;     // from pitch bend, in hue steps (256 to a turn)
float vibratoDepth = 0;  // 0..1, from controller 1
float vibratoNow = 0;    // where the swing is in this frame, in hue steps
uint8_t steadyShift = 0; // pitch bend only: the hue shift without the swing
uint8_t hueShift = 0;    // pitch bend plus the swing; added to every hue, wrapping round the wheel

// Works out this frame's hue shift. Call once a frame, before drawing.
void updateHueShift(uint32_t now) {
  static uint32_t lastMs = now;
  static float phase = 0;  // 0..1, one swing
  phase += (float)(now - lastMs) * VIBRATO_HZ / 1000.0f;
  phase -= floorf(phase);
  lastMs = now;

  vibratoNow = sinf(TWO_PI * phase) * vibratoDepth * VIBRATO_HUE_RANGE;
  // Turning the result into a uint8_t wraps a turn to the left (negative)
  // round the colour wheel.
  steadyShift = (uint8_t)(int)lroundf(bendShift);
  hueShift = (uint8_t)(int)lroundf(bendShift + vibratoNow);
}

// A colour in red, green and blue, 0-255 each, kept as fractions while several
// lights are being mixed. It may run past 255 on the way; toLeds() limits it.
struct Colour {
  float r, g, b;
};

// The colour of a note at full level. `shift` turns it round the wheel.
Colour colourOfNote(uint8_t note, uint8_t shift) {
  uint8_t hue = BASE_HUE;
  uint8_t saturation = BASE_SATURATION;
  float gain = COLOUR_GAIN;
  if (LOOK == Look::NoteNames) {
    hue = (uint8_t)(NOTE_HUE_OF_C + (note % 12) * 256 / 12);
    saturation = 255;
    gain = 1;
  }
  const CRGB full = CHSV((uint8_t)(hue + shift), saturation, 255);
  return {full.r * gain, full.g * gain, full.b * gain};
}

// ...with this frame's pitch bend and vibrato swing.
Colour colourOfNote(uint8_t note) { return colourOfNote(note, hueShift); }

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
// Keys from the preset's organFromNote up play an organ, which sounds for as long as the
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

// One patch of sparkle: `count` LEDs in a row, starting at `leds`, with their
// own `levels` (0..1) and `hues`. Starts `perSecond` new sparkles a second
// somewhere in the patch, fades the old ones, and adds them all on top of
// what is already drawn there. A new sparkle takes any colour of the wheel,
// or with `hueSpan` one of that many hue steps from `hueFrom`. `brightest`
// (0..1) scales how bright a new one starts.
void sparklePatch(CRGB* leds, float* levels, uint8_t* hues, uint8_t count, float perSecond, float elapsedMs,
                  uint8_t hueFrom = 0, uint8_t hueSpan = 0, float brightest = 1.0f) {
  // A preset that was not being drawn for a while comes back with one very
  // long frame. Without this limit that frame would start every sparkle it
  // had missed, all at once: a flash.
  if (elapsedMs > 100) elapsedMs = 100;

  // New sparkles for this frame. The fractional part becomes a probability.
  float due = perSecond * elapsedMs / 1000.0f;
  while (due > 0) {
    if (due >= 1 || random8() < (uint8_t)(due * 255)) {
      const uint8_t led = random8(count);
      levels[led] = brightest * (0.5f + random8() / 510.0f);  // half to all of it
      hues[led] = hueSpan > 0 ? (uint8_t)(hueFrom + random8(hueSpan)) : random8();
    }
    due -= 1;
  }

  // Fade the old ones and add them on top of what is already drawn.
  const float keep = expf(-elapsedMs / SPARKLE_FADE_MS);
  for (uint8_t i = 0; i < count; i++) {
    float& level = levels[i];
    if (level == 0) continue;
    level *= keep;
    if (level < 0.02f) {
      level = 0;
      continue;
    }
    const uint8_t value = (uint8_t)(255 * level);
    if (LOOK == Look::NoteNames) {
      leds[i] += CHSV((uint8_t)(hues[i] + hueShift), SPARKLE_SATURATION_COLOUR, value);
    } else {
      leds[i] += CHSV((uint8_t)(BASE_HUE + hueShift), SPARKLE_SATURATION, value);
    }
  }
}

// How far the joystick is up, 0..1, in a preset where that makes things
// sparkle; 0 in any other.
float sparkleFromJoystick() { return PRESETS[presetNow].sparkleOnJoystick ? vibratoDepth : 0.0f; }

void drawSparkle(uint8_t notesSounding, float elapsedMs) {
  float rate = 0;
  if (notesSounding >= SPARKLE_FROM_NOTES) {
    const float span = SPARKLE_FULL_AT_NOTES - SPARKLE_FROM_NOTES + 1;
    rate = SPARKLES_PER_SECOND * min(1.0f, (notesSounding - SPARKLE_FROM_NOTES + 1) / span);
  }
  // The joystick asks for sparkle too. The lapels show whichever asks for
  // more, not both added up.
  rate = fmaxf(rate, SPARKLES_PER_SECOND * sparkleFromJoystick());
  sparklePatch(lapelOne, sparkle[0], sparkleHue[0], LEDS_PER_LAPEL, rate / 2, elapsedMs);
  sparklePatch(lapelTwo, sparkle[1], sparkleHue[1], LEDS_PER_LAPEL, rate / 2, elapsedMs);
}

// Adds the organ keys' wash to every row.
void addOrganWash(Colour rows[], uint32_t now, float elapsedMs) {
  Colour mix = {0, 0, 0};
  float total = 0;
  for (uint8_t note = PRESETS[presetNow].organFromNote; note < 128; note++) {
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

// ---- the collar
//
// The collar does not ripple or sparkle; the lapels are the main event. While
// notes sound it glows, and every note sounding (keys held, and notes the
// pedal is holding) has its own band of the collar in its own colour, as
// bright as that note. One note has the whole collar. A chord divides it:
// each half of the collar is shared out equally, the lowest note at the
// collar bottom next to the lapel top, the highest at the back of the neck,
// so the two sides mirror each other. (Until 19 Nov a chord showed the
// average of its colours, which went pale: red, yellow-green and blue
// average out near white.) When notes come and go the bands re-divide, and
// every pixel fades to its new colour instead of jumping.
// It comes up quickly and falls away slowly. While the cape is looking
// for the keyboard, a short mark at each collar bottom blinks with the hem
// row, and the green sweep that says "connected" carries on from the lapel
// tops round the collar to meet at the back. It sits right under the
// wearer's face, so it is the quietest light on the cape: COLLAR_LEVEL is the
// most it ever shows, as a fraction of the brightness ceiling.
//
// In a preset with collarOnPressure all of that waits for aftertouch: the
// collar is dark while you simply play. Press down harder on held keys and,
// once the pressure reaches COLLAR_PRESSURE_ON, the collar opens: it comes
// up quickly and shows the bands as above. It stays open while the pressure
// wavers, and closes when the pressure is back at 0, falling away more
// slowly than it came. It also closes when no key is held any more, in case
// the keyboard's last pressure message was not a 0.
//
// The strip: 160 pixels a metre, WS2812B, green-red-blue order like the
// lapels, input at the socket end. On the bench it is still the whole uncut
// 2 m (320 pixels). The collar is the first COLLAR_PIXELS from the input end.
// The rest is sent black once, at start-up, and holds that; sending all 320
// every frame would halve the frame rate. On the mock cape the
// path is 80 cm: 15 cm up one side of the collar, 50 cm round the top, 15 cm
// down the other side (measured 19 Nov).
const uint8_t COLLAR_PIN = 6;              // output 3 on the shifter board
const uint16_t COLLAR_STRIP_PIXELS = 320;  // everything plugged into output 3
const uint16_t COLLAR_PIXELS = 128;        // the part that is the collar: 80 cm
const float COLLAR_LEVEL = 0.30f;          // the glow at its brightest, 0..1 of the ceiling
const float COLLAR_ATTACK_MS = 80;         // the glow comes up this fast
const float COLLAR_RELEASE_MS = 150;       // and falls away this fast
const uint8_t COLLAR_PRESSURE_ON = 10;     // collarOnPressure: aftertouch of this much (0-127) opens the collar
const float COLLAR_CLOSE_MS = 350;         // collarOnPressure: and it falls away this fast when it closes
const uint8_t COLLAR_MARK_PIXELS = 4;      // length of the link-state marks, about 2.5 cm
const uint32_t COLLAR_SWEEP_MS = 400;      // the connected sweep takes this long round the collar

CRGB collar[COLLAR_STRIP_PIXELS];
// What one side of the collar shows now, pixel by pixel from the collar bottom
// to the back of the neck, each moving towards its band's colour. The other
// side is the mirror image.
const uint16_t COLLAR_HALF = COLLAR_PIXELS / 2;
Colour collarNow[COLLAR_HALF] = {};
bool collarOpen = false;  // collarOnPressure: aftertouch has opened the collar

// Why the collar needs help to fade smoothly. An LED takes whole numbers,
// 0-255, and the brightness ceiling scales everything down to 0-32. The
// collar's glow tops out near 10 of those steps, so a plain fade would come
// down a staircase of ten visible steps, with each colour channel stepping
// at a different moment.
//
// The cure is dithering, in two ways at once. In space: to show 4.5 steps,
// every other pixel shows 4 and the ones between show 5, and the strip's
// coating and the tube blend them. In time: for the quarter steps a pixel
// swaps between its two values on alternate frames, about 60 times a second,
// and the eye takes the average. Together that is four times as many levels:
// about 40 where there were 10. The pattern never moves along the strip.
//
// The frame rate is what makes the swapping invisible. Earlier versions
// (19 Nov) ran at about 80 frames a second, because all 320 pixels of the
// uncut strip went out every frame, and Mike could see them shimmer and
// crawl. A pattern with four frames to its cycle is too slow at any rate this
// board reaches. FastLED's own dithering is off (see capeBegin), so the two
// do not fight.
//
// One channel of one pixel: `value` is the wanted level, 0-255 before the
// ceiling; `threshold` (0..1) decides whether this pixel rounds up.
uint8_t ditheredChannel(float value, float threshold) {
  // The level in the LED's own steps after the ceiling, e.g. 4.25.
  const float steps = fminf(255.0f, value) * MAX_BRIGHTNESS / 256.0f;
  uint16_t whole = (uint16_t)steps;
  if (steps - whole > threshold) whole++;
  // The smallest value that the ceiling turns into exactly `whole` steps.
  const uint16_t out = (whole * 256 + MAX_BRIGHTNESS - 1) / MAX_BRIGHTNESS;
  return out > 255 ? 255 : (uint8_t)out;
}

// Lights a short mark on each side of the collar, `offset` pixels in from its
// two ends. The ends are the collar bottoms, next to the lapel tops.
void collarMarks(uint16_t offset, const CRGB& colour) {
  for (uint8_t i = 0; i < COLLAR_MARK_PIXELS; i++) {
    const uint16_t pixel = offset + i;
    if (pixel >= COLLAR_PIXELS / 2) break;
    collar[pixel] = colour;
    collar[COLLAR_PIXELS - 1 - pixel] = colour;
  }
}

void drawCollar(uint32_t now) {
  static uint32_t lastDrawMs = now;
  const float elapsed = (float)(now - lastDrawMs);
  lastDrawMs = now;

  // The notes sounding, lowest first, each with a weight, 0..1: how hard a
  // held key was struck, or how much of its glow the pedal is still holding.
  uint8_t notes[COLLAR_HALF];
  float weights[COLLAR_HALF];
  uint8_t count = 0;
  bool keyHeld = false;
  for (uint8_t note = 0; note < 128 && count < COLLAR_HALF; note++) {
    if (heldVelocity[note] > 0) keyHeld = true;
    float weight = heldVelocity[note] / 127.0f;
    if (weight == 0 && sustainLevel[note] > 0) {
      weight = sustainLevel[note] / HELD_GLOW * (pedalDown ? pedalDepth : 1.0f);
    }
    if (weight == 0) continue;
    notes[count] = note;
    weights[count] = fminf(1.0f, weight);
    count++;
  }

  // A collar that waits for aftertouch shows nothing while it is closed.
  const bool waits = PRESETS[presetNow].collarOnPressure;
  if (!keyHeld) collarOpen = false;
  const bool closed = waits && !collarOpen;
  if (closed) count = 0;

  // Each band's colour at its note's level.
  Colour bands[COLLAR_HALF];
  for (uint8_t band = 0; band < count; band++) {
    bands[band] = {0, 0, 0};
    addColour(bands[band], colourOfNote(notes[band], steadyShift), COLLAR_LEVEL * weights[band]);
  }

  // The vibrato swing. The fading below is too slow to follow six swings a
  // second (it would smooth them away), so the bands are worked out without
  // the swing and each pixel's colour is turned afterwards. Turning a colour
  // is a rotation about the grey line (red = green = blue), which keeps red +
  // green + blue the same. These are that rotation's three numbers.
  const float angle = vibratoNow * TWO_PI / 256.0f;
  const float third = (1.0f - cosf(angle)) / 3.0f;
  const float lean = sinf(angle) * 0.57735f;  // 1 / square root of 3
  const float keepPart = cosf(angle) + third;
  const float fromPrevious = third + lean;  // red into green, green into blue, blue into red
  const float fromNext = third - lean;

  // Four thresholds, a quarter of a step apart: two for the even pixels and
  // two for the odd ones, one per frame of a pair. At a half step, the even
  // pixels are up in one frame and the odd pixels in the other.
  static const float THRESHOLD[2][2] = {{0.125f, 0.625f}, {0.875f, 0.375f}};
  static uint8_t frame = 0;
  frame ^= 1;

  const float attack = 1.0f - expf(-elapsed / COLLAR_ATTACK_MS);
  const float release = 1.0f - expf(-elapsed / (closed ? COLLAR_CLOSE_MS : COLLAR_RELEASE_MS));
  const Colour dark = {0, 0, 0};
  for (uint16_t i = 0; i < COLLAR_HALF; i++) {
    // Which band this pixel is in, and so the colour it is heading for.
    const Colour& target = count == 0 ? dark : bands[i * count / COLLAR_HALF];
    Colour& shown = collarNow[i];
    const bool rising = target.r + target.g + target.b > shown.r + shown.g + shown.b;
    const float step = rising ? attack : release;
    shown.r += (target.r - shown.r) * step;
    shown.g += (target.g - shown.g) * step;
    shown.b += (target.b - shown.b) * step;

    // Turned by the vibrato swing. A pure colour can come out a little below
    // zero in one channel; that is limited to zero.
    const Colour turned = {
        fmaxf(0.0f, shown.r * keepPart + shown.g * fromNext + shown.b * fromPrevious),
        fmaxf(0.0f, shown.g * keepPart + shown.b * fromNext + shown.r * fromPrevious),
        fmaxf(0.0f, shown.b * keepPart + shown.r * fromNext + shown.g * fromPrevious),
    };

    // The pixel on this side, and its mirror image on the other.
    const uint16_t pixels[2] = {i, (uint16_t)(COLLAR_PIXELS - 1 - i)};
    for (const uint16_t pixel : pixels) {
      const float threshold = THRESHOLD[pixel & 1][frame];
      collar[pixel] = CRGB(ditheredChannel(turned.r, threshold), ditheredChannel(turned.g, threshold),
                           ditheredChannel(turned.b, threshold));
    }
  }
}

void collarDark() {
  memset(collarNow, 0, sizeof(collarNow));
  fill_solid(collar, COLLAR_PIXELS, CRGB::Black);
}

// ---- the strings below the lapels
//
// A struck key sends a wavefront down the lapel. When it reaches the bottom
// it runs off into the string: one dot of the note's colour travels along
// the string's lights, away from the lapel, fading as it goes. It starts as
// bright as the wavefront was when it left the lapel, so low notes, which
// start near the bottom, send bright dots; a note from the middle of the
// keyboard sends a faint one; and the ripple of a high note dies on the lapel
// and sends none. Both strings show the same. With nothing played they are
// dark.
//
// Light 0 is the one next to the lapel. As placed on the mock cape (20 Nov),
// each string runs:
//     6 lights  down the front edge, below the lapel
//     4 lights  across, above the hem: the crossing, always dark
//     7 lights  up the outer line
//     the rest  (33) coiled at the top of the outer line, always dark
// A dot keeps its speed through the crossing, unseen: it goes out at the
// bottom of the front edge and comes back a third of a second later at the
// bottom of the outer line. It lasts just long enough to reach the top of the
// outer line. The dark lights are still sent black every frame: a light that
// gets no data is not reliably dark.
//
// In a preset with dotsFullTrip the key decides neither whether there is a
// dot nor how far it can be followed. Every key sends one. It starts as
// bright as the key was struck, however far up the lapel the key is, and it
// fades only part of the way, so it is still lit when it runs off the top of
// the outer line. A key further up still sends its dot later, when its
// wavefront reaches the bottom of the lapel; for the highest keys (above G5),
// whose wavefront dies on the way, the dot starts at the moment the ripple
// dies, 0.7 s after the key.
const uint8_t STRAND_FRONT_LIGHTS = 6;
const uint8_t STRAND_CROSSING_LIGHTS = 4;
const uint8_t STRAND_OUTER_LIGHTS = 7;
const uint8_t STRAND_OUTER_FROM = STRAND_FRONT_LIGHTS + STRAND_CROSSING_LIGHTS;
const uint8_t STRAND_PATH_LIGHTS = STRAND_OUTER_FROM + STRAND_OUTER_LIGHTS;

// True for the lights that may show: the front edge and the outer line.
bool strandLightShows(uint8_t light) {
  return light < STRAND_FRONT_LIGHTS || (light >= STRAND_OUTER_FROM && light < STRAND_PATH_LIGHTS);
}

const float RUNNER_LIFE_MS = 1500;          // how long a dot lasts: 18 lights' travel, the path is 17
const float RUNNER_LIGHTS_PER_MS = 0.012f;  // how fast it travels: 12 lights, 1.2 m, a second
const float RUNNER_WIDTH_LIGHTS = 1.0f;     // 1 = passed smoothly from each light to the next
const float RUNNER_FULL_TRIP_END = 0.35f;   // with dotsFullTrip: what is left of a dot at the end of its life

struct Runner {
  bool active;
  uint8_t note;      // decides the colour
  float strength;    // 0..1 as it leaves the lapel
  uint32_t startMs;  // when it leaves the lapel; in the future while the wavefront is still on its way
};
// Room for this many at a time in each list. A dot can be up to 2.2 s old
// (0.7 s waiting, 1.5 s travelling), so 32 covers 14 keys a second.
const uint8_t RUNNER_SLOTS = 32;
Runner runners[RUNNER_SLOTS];          // dots on the strings
Runner shoulderRunners[RUNNER_SLOTS];  // wavefronts on the shoulders (see "the shoulders" below)

// Books a runner in `list` for a wavefront that needs `travelMs` to reach the
// end of the lapel. The runner starts as strong as the wavefront is by then,
// and a wavefront that dies on the lapel books nothing. With `always` every
// key books one, as strong as the key was struck; where the wavefront dies
// on the lapel, the runner starts at that moment.
void bookRunner(Runner (&list)[RUNNER_SLOTS], uint8_t note, float travelMs, float strength, uint32_t struckMs,
                bool always) {
  if (travelMs >= RIPPLE_LIFE_MS) {
    if (!always) return;
    travelMs = RIPPLE_LIFE_MS;
  }
  const float remaining = always ? 1.0f : 1.0f - travelMs / RIPPLE_LIFE_MS;

  // Reuse a finished runner, or failing that the oldest one.
  Runner* slot = &list[0];
  for (Runner& runner : list) {
    if (!runner.active) {
      slot = &runner;
      break;
    }
    if ((int32_t)(runner.startMs - slot->startMs) < 0) slot = &runner;
  }
  slot->active = true;
  slot->note = note;
  slot->strength = strength * remaining * remaining;  // the ripple's own fade
  slot->startMs = struckMs + (uint32_t)travelMs;
}

// A key was struck at `row` with `strength`. Its ripple has two wavefronts:
// the one going down becomes a dot on the string, the one going up becomes a
// wavefront on the shoulder.
void launchRunners(uint8_t note, float row, float strength, uint32_t struckMs) {
  bookRunner(runners, note, row / RIPPLE_ROWS_PER_MS, strength, struckMs, PRESETS[presetNow].dotsFullTrip);
  bookRunner(shoulderRunners, note, ((ROWS - 1) - row) / RIPPLE_ROWS_PER_MS, strength, struckMs, false);
}

void strandsDark() {
  fill_solid(lapelOne + LEDS_PER_LAPEL, STRAND_LIGHTS, CRGB::Black);
  fill_solid(lapelTwo + LEDS_PER_LAPEL, STRAND_LIGHTS, CRGB::Black);
}

void drawStrands(uint32_t now) {
  // How much of a dot is left at the end of its life: nothing, or with
  // dotsFullTrip enough to be seen all the way.
  const float endLevel = PRESETS[presetNow].dotsFullTrip ? RUNNER_FULL_TRIP_END : 0.0f;
  Colour lights[STRAND_LIGHTS] = {};
  for (Runner& runner : runners) {
    if (!runner.active) continue;
    const int32_t age = (int32_t)(now - runner.startMs);
    if (age < 0) continue;  // its wavefront is still on the lapel
    if (age >= RUNNER_LIFE_MS) {
      runner.active = false;
      continue;
    }
    // A steady fade over the dot's life. (Until 20 Nov it fell away faster,
    // with the square of what remained; on the placed path that left the
    // outer line almost dark. The start is no brighter than before.)
    const float remaining = 1.0f - (1.0f - endLevel) * age / RUNNER_LIFE_MS;
    const float level = runner.strength * remaining;
    const float position = age * RUNNER_LIGHTS_PER_MS;
    const Colour colour = colourOfNote(runner.note);
    for (uint8_t light = 0; light < STRAND_PATH_LIGHTS; light++) {
      if (!strandLightShows(light)) continue;
      const float distance = fabsf((float)light - position);
      if (distance < RUNNER_WIDTH_LIGHTS) {
        addColour(lights[light], colour, level * (1.0f - distance / RUNNER_WIDTH_LIGHTS));
      }
    }
  }
  for (uint8_t light = 0; light < STRAND_LIGHTS; light++) {
    const CRGB colour = toLeds(lights[light]);
    lapelOne[LEDS_PER_LAPEL + light] = colour;
    lapelTwo[LEDS_PER_LAPEL + light] = colour;
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

// ---- the shoulders
//
// Epaulettes: on each shoulder two rows of 12 LEDs of the lapel strip, side by
// side, built like a short lapel. Row one runs from the neck out to the
// shoulder edge (LEDs 0-11), a U-turn, then row two runs back to the neck
// (12-23). So a shoulder "row" (0 at the neck, 11 at the edge) is two LEDs:
// `row` and `23 - row`. Both shoulders are chained on output 4: the cable from
// the board reaches shoulder one at its neck end, and a short link behind the
// neck carries on from the end of its row two to shoulder two.
//
// What they show is the mirror of the strings. A struck key also sends a
// wavefront up the lapel. When it reaches the top it runs off onto the
// shoulders: a wavefront of the note's colour crosses each epaulette from the
// neck to the edge, fading as it goes. It starts as bright as the lapel's
// wavefront was when it left, so high notes send bright ones, a note from the
// middle of the keyboard a faint one, and low notes none. Organ keys do not
// ripple, so they send none either. Both shoulders show the same. With
// nothing played they are dark.
//
// In a preset with shouldersOnChord they also come alive on a big chord.
// Once SHOULDER_ALIVE_KEYS or more keys (keys, not notes the pedal is
// holding) have been down together for SHOULDER_ALIVE_AFTER_MS, and for as
// long as that many stay down, every light of both epaulettes is on, in one
// colour. A chord that is struck and let go sooner does nothing here, and
// dropping below that many keys, even for a moment, starts the wait again. It
// comes up quickly, at one level however hard the keys were struck. The
// colour is picked at random each time they come alive: a pure colour of the
// wheel, so never white or pale, and at least a quarter of the wheel away
// from the last one, so never the same twice running. It holds while keys
// are added or swapped, as long as enough stay down; pitch bend and vibrato
// do not turn it. With fewer keys held it fades out, more slowly. (For an
// hour on 21 Nov the rows were shared out among the held keys, each in its
// own colour; Mike wanted one solid colour.) The wavefronts carry on
// throughout; where one crosses the glow, a light shows the brighter of the
// two, not their sum.
//
// The ruler that tested the wiring is in git history (drawShoulderRuler,
// commit 8748bdb and before).
const uint8_t SHOULDER_PIN = 7;  // output 4 on the shifter board
const uint8_t SHOULDER_ROWS = 12;
const uint8_t LEDS_PER_SHOULDER = 2 * SHOULDER_ROWS;
const uint8_t SHOULDER_PIXELS = 2 * LEDS_PER_SHOULDER;
CRGB shoulders[SHOULDER_PIXELS];

const float SHOULDER_LIFE_MS = 450;          // how long a wavefront lasts: just past the edge
const float SHOULDER_ROWS_PER_MS = 0.030f;   // how fast it crosses: 30 rows, half a metre, a second
const float SHOULDER_WIDTH_ROWS = 2.0f;      // its softness

const uint8_t SHOULDER_ALIVE_KEYS = 4;        // shouldersOnChord: this many keys held brings them alive
const uint32_t SHOULDER_ALIVE_AFTER_MS = 500; // once they have been held this long
const float SHOULDER_ALIVE_LEVEL = 0.35f;     // how bright then, 0..1 of the ceiling
const float SHOULDER_ALIVE_ATTACK_MS = 60;    // it comes up this fast
const float SHOULDER_ALIVE_RELEASE_MS = 300;  // and falls away this fast
// The chord glow each row shows now, moving towards what it should show.
Colour shoulderGlow[SHOULDER_ROWS] = {};

void shouldersDark() {
  memset(shoulderGlow, 0, sizeof(shoulderGlow));
  fill_solid(shoulders, SHOULDER_PIXELS, CRGB::Black);
}

// Moves the chord glow one frame on.
void updateShoulderGlow(uint32_t now, float elapsedMs) {
  if (!PRESETS[presetNow].shouldersOnChord) {
    memset(shoulderGlow, 0, sizeof(shoulderGlow));
    return;
  }
  uint8_t count = 0;  // keys held
  for (uint8_t note = 0; note < 128 && count < SHOULDER_ALIVE_KEYS; note++) {
    if (heldVelocity[note] > 0) count++;
  }
  // Enough keys, and for long enough?
  static bool enough = false;
  static uint32_t enoughSinceMs = 0;
  const bool enoughNow = count >= SHOULDER_ALIVE_KEYS;
  if (enoughNow && !enough) enoughSinceMs = now;
  enough = enoughNow;
  const bool alive = enough && now - enoughSinceMs >= SHOULDER_ALIVE_AFTER_MS;

  // A new colour each time they come alive: 64 to 192 steps round the
  // 256-step wheel from the last one. random() here draws on the chip's
  // hardware random numbers, so the order differs every time.
  static bool wasAlive = false;
  static uint8_t hue = (uint8_t)random(256);
  if (alive && !wasAlive) hue += (uint8_t)random(64, 193);
  wasAlive = alive;
  const CRGB pure = CHSV(hue, 255, 255);
  const Colour colour = {(float)pure.r, (float)pure.g, (float)pure.b};

  const float attack = 1.0f - expf(-elapsedMs / SHOULDER_ALIVE_ATTACK_MS);
  const float release = 1.0f - expf(-elapsedMs / SHOULDER_ALIVE_RELEASE_MS);
  for (uint8_t row = 0; row < SHOULDER_ROWS; row++) {
    Colour target = {0, 0, 0};
    if (alive) addColour(target, colour, SHOULDER_ALIVE_LEVEL);
    Colour& shown = shoulderGlow[row];
    const bool rising = target.r + target.g + target.b > shown.r + shown.g + shown.b;
    const float step = rising ? attack : release;
    shown.r += (target.r - shown.r) * step;
    shown.g += (target.g - shown.g) * step;
    shown.b += (target.b - shown.b) * step;
  }
}

void drawShoulders(uint32_t now) {
  static uint32_t lastDrawMs = now;
  updateShoulderGlow(now, (float)(now - lastDrawMs));
  lastDrawMs = now;

  Colour rows[SHOULDER_ROWS] = {};
  for (Runner& runner : shoulderRunners) {
    if (!runner.active) continue;
    const int32_t age = (int32_t)(now - runner.startMs);
    if (age < 0) continue;  // its wavefront is still on the lapel
    if (age >= SHOULDER_LIFE_MS) {
      runner.active = false;
      continue;
    }
    const float level = runner.strength * (1.0f - age / SHOULDER_LIFE_MS);
    const float position = age * SHOULDER_ROWS_PER_MS;
    const Colour colour = colourOfNote(runner.note);
    for (uint8_t row = 0; row < SHOULDER_ROWS; row++) {
      const float distance = fabsf((float)row - position);
      if (distance < SHOULDER_WIDTH_ROWS) {
        addColour(rows[row], colour, level * (1.0f - distance / SHOULDER_WIDTH_ROWS));
      }
    }
  }
  for (uint8_t shoulder = 0; shoulder < 2; shoulder++) {
    CRGB* leds = shoulders + shoulder * LEDS_PER_SHOULDER;
    for (uint8_t row = 0; row < SHOULDER_ROWS; row++) {
      Colour both = rows[row];
      raiseColour(both, shoulderGlow[row], 1.0f);
      const CRGB colour = toLeds(both);
      leds[row] = colour;
      leds[LEDS_PER_SHOULDER - 1 - row] = colour;
    }
  }
}

// ---- sparkle from the joystick, below and above the lapels
//
// In a preset with sparkleOnJoystick, pushing the joystick up (controller 1,
// the mod wheel; on the Kronos it adds vibrato) makes the cape sparkle for as
// long as it is up, and more the further it is pushed: the lapels (see
// drawSparkle), the lights of the strings that show, and the shoulders. Not
// the collar. A sparkle is one light flashing briefly in a colour of its own,
// on top of whatever that light is showing. It needs no note to be sounding.
// The two strings sparkle each for itself, and so do the two shoulders.
const float STRAND_SPARKLES_PER_SECOND = 10;    // at full joystick, both strings together
const float SHOULDER_SPARKLES_PER_SECOND = 12;  // at full joystick, both shoulders together

float strandSparkle[2][STRAND_PATH_LIGHTS];  // 0..1, per string, per light on the path
uint8_t strandSparkleHue[2][STRAND_PATH_LIGHTS];
float shoulderSparkle[SHOULDER_PIXELS];
uint8_t shoulderSparkleHue[SHOULDER_PIXELS];

void joystickSparkleOff() {
  memset(strandSparkle, 0, sizeof(strandSparkle));
  memset(shoulderSparkle, 0, sizeof(shoulderSparkle));
}

// Call after the strings and the shoulders are drawn.
void drawJoystickSparkle(uint32_t now) {
  static uint32_t lastDrawMs = now;
  const float elapsed = (float)(now - lastDrawMs);
  lastDrawMs = now;
  const float depth = sparkleFromJoystick();

  // Each string in two patches, the front edge and the outer line, so that
  // the crossing between them stays dark. Each patch gets its share of the
  // rate by its number of lights.
  const float perLight = STRAND_SPARKLES_PER_SECOND * depth / (2 * (STRAND_FRONT_LIGHTS + STRAND_OUTER_LIGHTS));
  CRGB* const strings[2] = {lapelOne + LEDS_PER_LAPEL, lapelTwo + LEDS_PER_LAPEL};
  for (uint8_t s = 0; s < 2; s++) {
    sparklePatch(strings[s], strandSparkle[s], strandSparkleHue[s], STRAND_FRONT_LIGHTS,
                 perLight * STRAND_FRONT_LIGHTS, elapsed);
    sparklePatch(strings[s] + STRAND_OUTER_FROM, strandSparkle[s] + STRAND_OUTER_FROM,
                 strandSparkleHue[s] + STRAND_OUTER_FROM, STRAND_OUTER_LIGHTS, perLight * STRAND_OUTER_LIGHTS,
                 elapsed);
  }

  sparklePatch(shoulders, shoulderSparkle, shoulderSparkleHue, SHOULDER_PIXELS,
               SHOULDER_SPARKLES_PER_SECOND * depth, elapsed);
}

// ---- the Purple Rain preset
//
// Rain, in purples, and nothing else: no ripples, no note colours.
//
// Drops fall. Each lapel is two columns of LEDs, so there are four columns
// of rain on the front, each for itself. A drop starts at the top of a
// column, falls to the hem with a short tail behind it, and carries on down
// that side's string along the front edge. The outer line of each string has
// drops of its own, falling from its top to its bottom.
//
// How hard it rains:
//     nothing played    a thin, faint drizzle
//     notes sounding    steady rain, heavier with every note (held keys, and
//                       notes the pedal is holding), up to RAIN_NOTES_MOST
//     a key struck      one drop at once, as bright as the key was struck, a
//                       little paler than the rest so that it stands out;
//                       low keys lean to blue-violet, high keys to magenta
//     joystick up       a downpour, heavier the further it is pushed
//
// The shoulders: drops landing. Single lights flash and fade, as often as it
// is raining, and one for every key struck.
//
// The collar: the cloud. Dark with nothing played; a deep violet gathers
// slowly while notes sound and thins slowly afterwards, with a slow drift
// along it so that it is not a flat band.
//
// Pitch bend and the joystick do not turn the colours here. It stays purple.
const float RAIN_ROW_CM = 100.0f / 60;                 // the lapel strip has 60 LEDs a metre
const float RAIN_LIGHT_CM = 10;                        // the string's lights are 10 cm apart
const float RAIN_LAPEL_CM = (ROWS - 1) * RAIN_ROW_CM;  // from the top row to the hem row
const float RAIN_TAIL_CM = 9;                          // a drop's tail on the lapel
const float RAIN_SLOWEST_CM_PER_MS = 0.055f;           // each drop has its own speed: from 55 cm a second
const float RAIN_FASTEST_CM_PER_MS = 0.095f;           // to 95
const uint8_t RAIN_HUE_FROM = 178;                     // from blue-violet (192 is purple on the 256-step wheel)
const uint8_t RAIN_HUE_SPAN = 30;                      // to purple leaning towards magenta
const float RAIN_DRIZZLE_PER_SECOND = 0.6f;            // drops a second with nothing played, on the whole cape
const float RAIN_PER_NOTE_PER_SECOND = 2.0f;           // more for each note sounding
const uint8_t RAIN_NOTES_MOST = 6;                     // notes beyond this add no more
const float RAIN_STORM_PER_SECOND = 14;                // more again at full joystick
const float RAIN_PATTER_SHARE = 0.5f;                  // shoulder flashes for each drop of rain
const uint8_t RAIN_CLOUD_HUE = 186;                    // the collar: deep violet
const float RAIN_CLOUD_LEVEL = 0.22f;                  // at its fullest, 0..1 of the ceiling
const uint8_t RAIN_CLOUD_FULL_AT_NOTES = 4;            // this many notes sounding gather the whole cloud
const float RAIN_CLOUD_ATTACK_MS = 500;                // it gathers this fast
const float RAIN_CLOUD_RELEASE_MS = 1800;              // and thins this fast

// Where a drop falls. Lanes 0-3 are the lapel columns (lapel one's two, then
// lapel two's), each running on down that lapel's string. Lanes 4 and 5 are
// the outer lines of string one and string two.
struct Drop {
  bool active;
  uint8_t lane;
  float headCm;   // how far its head has fallen from the top of its lane
  float cmPerMs;  // its speed
  float level;    // 0..1
  Colour colour;
};
Drop drops[48];

void rainOff() {
  for (Drop& drop : drops) drop.active = false;
}

// Starts a drop at the top of a lane picked at random. With every drop in use
// it does nothing, unless it `must`: then the drop furthest along gives way.
void startDrop(float level, uint8_t hue, uint8_t saturation, bool must) {
  Drop* slot = nullptr;
  for (Drop& drop : drops) {
    if (!drop.active) {
      slot = &drop;
      break;
    }
  }
  if (slot == nullptr) {
    if (!must) return;
    slot = &drops[0];
    for (Drop& drop : drops) {
      if (drop.headCm > slot->headCm) slot = &drop;
    }
  }
  // A lapel column is twice as likely as an outer line: it is the longer way.
  const uint8_t pick = random8(10);
  slot->active = true;
  slot->lane = pick < 8 ? pick / 2 : 4 + (pick - 8);
  slot->headCm = -RAIN_ROW_CM;  // just above the top light
  slot->cmPerMs = RAIN_SLOWEST_CM_PER_MS + (RAIN_FASTEST_CM_PER_MS - RAIN_SLOWEST_CM_PER_MS) * random8() / 255.0f;
  slot->level = level;
  const CRGB pure = CHSV(hue, saturation, 255);
  slot->colour = {(float)pure.r, (float)pure.g, (float)pure.b};
}

// A key was struck: its own drop, and one landing on a shoulder.
void rainStrike(uint8_t note, uint8_t velocity) {
  const float level = fmaxf(0.4f, velocity / 127.0f);
  const uint8_t hue = RAIN_HUE_FROM + (uint8_t)(rowOfNote(note) * RAIN_HUE_SPAN / ROWS);
  startDrop(level, hue, 200, true);
  const uint8_t led = random8(SHOULDER_PIXELS);
  shoulderSparkle[led] = 0.8f * level;
  shoulderSparkleHue[led] = hue;
}

// How much of a drop a light shows. `behindCm` is how far the light is behind
// the drop's head: positive in the tail, negative ahead of the head, where the
// light comes up over `leadCm` so that the head glides instead of hopping.
float dropShape(float behindCm, float leadCm, float tailCm) {
  const float part = behindCm < 0 ? 1.0f + behindCm / leadCm : 1.0f - behindCm / tailCm;
  return part > 0 ? part : 0;
}

// The collar as the cloud. `gathered` is 0..1.
void drawRainCloud(float elapsedMs, float gathered) {
  // Dithering, as in drawCollar: see the note above ditheredChannel.
  static const float THRESHOLD[2][2] = {{0.125f, 0.625f}, {0.875f, 0.375f}};
  static uint8_t frame = 0;
  frame ^= 1;
  static float drift = 0;  // where the slow drift is, in radians
  drift += elapsedMs * 0.0012f;
  if (drift > TWO_PI) drift -= TWO_PI;

  const CRGB pure = CHSV(RAIN_CLOUD_HUE, 255, 255);
  const float attack = 1.0f - expf(-elapsedMs / RAIN_CLOUD_ATTACK_MS);
  const float release = 1.0f - expf(-elapsedMs / RAIN_CLOUD_RELEASE_MS);
  for (uint16_t i = 0; i < COLLAR_HALF; i++) {
    // Thicker and thinner along the collar, moving slowly towards the back.
    const float level = RAIN_CLOUD_LEVEL * gathered * (0.7f + 0.3f * sinf(i * 0.2f - drift));
    const Colour target = {pure.r * level, pure.g * level, pure.b * level};
    Colour& shown = collarNow[i];
    const bool rising = target.r + target.g + target.b > shown.r + shown.g + shown.b;
    const float step = rising ? attack : release;
    shown.r += (target.r - shown.r) * step;
    shown.g += (target.g - shown.g) * step;
    shown.b += (target.b - shown.b) * step;

    const uint16_t pixels[2] = {i, (uint16_t)(COLLAR_PIXELS - 1 - i)};
    for (const uint16_t pixel : pixels) {
      const float threshold = THRESHOLD[pixel & 1][frame];
      collar[pixel] = CRGB(ditheredChannel(shown.r, threshold), ditheredChannel(shown.g, threshold),
                           ditheredChannel(shown.b, threshold));
    }
  }
}

// Draws every light of the cape for one frame.
void drawRain(uint32_t now) {
  static uint32_t lastDrawMs = now;
  float elapsed = (float)(now - lastDrawMs);
  lastDrawMs = now;
  // Coming back to this preset after a while must not count as one long
  // frame: that would start minutes' worth of rain at once.
  if (elapsed > 100) elapsed = 100;
  hueShift = 0;  // it stays purple

  // The notes sounding: keys held, and released notes while the pedal is down.
  if (!pedalDown) memset(sustainLevel, 0, sizeof(sustainLevel));
  uint8_t sounding = 0;
  for (uint8_t note = 0; note < 128; note++) {
    if (heldVelocity[note] > 0 || sustainLevel[note] > 0) sounding++;
  }

  // New drops for this frame. The fractional part becomes a probability.
  const float rate = RAIN_DRIZZLE_PER_SECOND + RAIN_PER_NOTE_PER_SECOND * min(sounding, RAIN_NOTES_MOST) +
                     RAIN_STORM_PER_SECOND * vibratoDepth;
  const bool raining = sounding > 0 || vibratoDepth > 0;
  float due = rate * elapsed / 1000.0f;
  while (due > 0) {
    if (due >= 1 || random8() < (uint8_t)(due * 255)) {
      // Drizzle is faint; rain is clearer.
      const float level = (raining ? 0.30f : 0.15f) + (raining ? 0.30f : 0.12f) * random8() / 255.0f;
      startDrop(level, RAIN_HUE_FROM + random8(RAIN_HUE_SPAN), 255, false);
    }
    due -= 1;
  }

  // Let the drops fall, and add each one to the lights of its lane.
  static Colour columns[4][ROWS];
  static Colour front[2][STRAND_FRONT_LIGHTS];
  static Colour outer[2][STRAND_OUTER_LIGHTS];
  memset(columns, 0, sizeof(columns));
  memset(front, 0, sizeof(front));
  memset(outer, 0, sizeof(outer));
  for (Drop& drop : drops) {
    if (!drop.active) continue;
    drop.headCm += drop.cmPerMs * elapsed;
    if (drop.lane < 4) {
      // Down the lapel column, then down the string's front edge. The first
      // string light is one light's spacing below the hem row.
      if (drop.headCm > RAIN_LAPEL_CM + RAIN_LIGHT_CM * (STRAND_FRONT_LIGHTS + 1)) {
        drop.active = false;
        continue;
      }
      for (uint8_t row = 0; row < ROWS; row++) {
        const float amount = dropShape(drop.headCm - (ROWS - 1 - row) * RAIN_ROW_CM, RAIN_ROW_CM, RAIN_TAIL_CM);
        if (amount > 0) addColour(columns[drop.lane][row], drop.colour, drop.level * amount);
      }
      for (uint8_t light = 0; light < STRAND_FRONT_LIGHTS; light++) {
        const float amount =
            dropShape(drop.headCm - (RAIN_LAPEL_CM + RAIN_LIGHT_CM * (light + 1)), RAIN_LIGHT_CM, RAIN_LIGHT_CM);
        if (amount > 0) addColour(front[drop.lane / 2][light], drop.colour, drop.level * amount);
      }
    } else {
      // Down the outer line: its top light is the last one on the path.
      if (drop.headCm > RAIN_LIGHT_CM * STRAND_OUTER_LIGHTS) {
        drop.active = false;
        continue;
      }
      for (uint8_t fromTop = 0; fromTop < STRAND_OUTER_LIGHTS; fromTop++) {
        const float amount = dropShape(drop.headCm - RAIN_LIGHT_CM * fromTop, RAIN_LIGHT_CM, RAIN_LIGHT_CM);
        if (amount > 0) {
          addColour(outer[drop.lane - 4][STRAND_OUTER_LIGHTS - 1 - fromTop], drop.colour, drop.level * amount);
        }
      }
    }
  }

  // Out to the lapels and the strings. The crossing and the spare lights of
  // each string are sent black, as always.
  CRGB* const lapels[2] = {lapelOne, lapelTwo};
  for (uint8_t lapel = 0; lapel < 2; lapel++) {
    for (uint8_t row = 0; row < ROWS; row++) {
      lapels[lapel][row] = toLeds(columns[2 * lapel][row]);
      lapels[lapel][LEDS_PER_LAPEL - 1 - row] = toLeds(columns[2 * lapel + 1][row]);
    }
    for (uint8_t light = 0; light < STRAND_LIGHTS; light++) {
      CRGB colour = CRGB::Black;
      if (light < STRAND_FRONT_LIGHTS) {
        colour = toLeds(front[lapel][light]);
      } else if (light >= STRAND_OUTER_FROM && light < STRAND_PATH_LIGHTS) {
        colour = toLeds(outer[lapel][light - STRAND_OUTER_FROM]);
      }
      lapels[lapel][LEDS_PER_LAPEL + light] = colour;
    }
  }

  // The shoulders: drops landing.
  fill_solid(shoulders, SHOULDER_PIXELS, CRGB::Black);
  sparklePatch(shoulders, shoulderSparkle, shoulderSparkleHue, SHOULDER_PIXELS, rate * RAIN_PATTER_SHARE, elapsed,
               RAIN_HUE_FROM, RAIN_HUE_SPAN, raining ? 0.7f : 0.35f);

  // The collar: the cloud.
  drawRainCloud(elapsed, fminf(1.0f, (float)sounding / RAIN_CLOUD_FULL_AT_NOTES));
}

// ---- which preset: the signal on the lapels
//
// Button two just chose a preset. For a little over a second the lapels show
// as many white bars as the preset's number, counted up from near the hem,
// on top of whatever else they are showing. The wearer sees the lapels by
// looking down; the collar would be out of sight.
const uint32_t PRESET_SIGNAL_MS = 1200;
const uint8_t PRESET_FIRST_ROW = 6;  // the first bar starts here
const uint8_t PRESET_BAR_ROWS = 3;   // each bar is this many rows
const uint8_t PRESET_BAR_STEP = 6;   // from the start of one bar to the start of the next

void drawPresetSignal(uint32_t now) {
  if (presetChosenMs == 0 || now - presetChosenMs >= PRESET_SIGNAL_MS) return;
  for (uint8_t bar = 0; bar <= presetNow; bar++) {
    for (uint8_t i = 0; i < PRESET_BAR_ROWS; i++) {
      const uint8_t row = PRESET_FIRST_ROW + bar * PRESET_BAR_STEP + i;
      if (row < ROWS) setRow(row, CRGB(70, 70, 70));
    }
  }
}

}  // namespace

uint8_t capePresetCount() { return PRESET_COUNT; }
uint8_t capePresetNumber() { return presetNow + 1; }
const char* capePresetName() { return PRESETS[presetNow].name; }

void capeNextPreset() {
  presetNow = (presetNow + 1) % PRESET_COUNT;
  settings.putUChar("preset", presetNow);
  presetChosenMs = millis();
  if (presetChosenMs == 0) presetChosenMs = 1;  // 0 means "not yet"
}

void capeBegin() {
  // The preset in use when the cape was last switched off.
  settings.begin("cape", false);
  presetNow = settings.getUChar("preset", 0);
  if (presetNow >= PRESET_COUNT) presetNow = 0;

  FastLED.addLeds<WS2812B, LAPEL_ONE_PIN, GRB>(lapelOne, LAPEL_CHAIN_PIXELS);
  FastLED.addLeds<WS2812B, LAPEL_TWO_PIN, GRB>(lapelTwo, LAPEL_CHAIN_PIXELS);
  CLEDController& collarStrip = FastLED.addLeds<WS2812B, COLLAR_PIN, GRB>(collar, COLLAR_STRIP_PIXELS);
  FastLED.addLeds<WS2812B, SHOULDER_PIN, GRB>(shoulders, SHOULDER_PIXELS);
  // FastLED's own dithering stays off everywhere. The collar does its own
  // (see ditheredChannel). The lapels have never had it: FastLED only dithers
  // at 100 frames a second or more, and the cape ran just under that until
  // 19 Nov. Switching it off keeps them looking as they did.
  FastLED.setDither(DISABLE_DITHER);
  // The two limits. Everything drawn goes through both.
  FastLED.setBrightness(MAX_BRIGHTNESS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, MAX_MILLIAMPS);
  // One black frame to everything plugged into output 3, then only the
  // collar's own pixels from here on. The pixels beyond keep that black.
  FastLED.clear(true);
  collarStrip.setLeds(collar, COLLAR_PIXELS);
}

void capeNoteOn(uint8_t note, uint8_t velocity) {
  note &= 0x7F;
  heldVelocity[note] = velocity;
  sustainLevel[note] = 0;  // struck again: the key's own glow takes over
  if (PRESETS[presetNow].rain) {  // no ripple: a raindrop
    rainStrike(note, velocity);
    return;
  }
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
  launchRunners(note, slot->row, slot->strength, slot->startMs);
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
  // bend is -8192..8191. updateHueShift() adds the vibrato swing.
  bendShift = bend * BEND_HUE_RANGE / 8192.0f;
}

void capeModulation(uint8_t value) { vibratoDepth = (value > 127 ? 127 : value) / 127.0f; }

void capeBlank() { FastLED.clear(true); }

namespace {
uint16_t framesThisSecond = 0;
uint16_t framesLastSecond = 0;
uint32_t secondStartedMs = 0;
}  // namespace

uint16_t capeFramesPerSecond() { return framesLastSecond; }

namespace {
uint32_t longestGapMs = 0;
}  // namespace

uint16_t capeLongestGapMs() {
  const uint32_t gap = longestGapMs;
  longestGapMs = 0;
  return gap > 65535 ? 65535 : (uint16_t)gap;
}

void capeChannelPressure(uint8_t value) {
  // Opens at COLLAR_PRESSURE_ON and closes only at 0, so pressure wavering
  // around the opening value does not make the collar flicker.
  if (value >= COLLAR_PRESSURE_ON) {
    collarOpen = true;
  } else if (value == 0) {
    collarOpen = false;
  }
}

void capeAllNotesOff() {
  bendShift = 0;
  vibratoDepth = 0;
  memset(heldVelocity, 0, sizeof(heldVelocity));
  memset(sustainLevel, 0, sizeof(sustainLevel));
  memset(organLevel, 0, sizeof(organLevel));
  memset(sparkle, 0, sizeof(sparkle));
  joystickSparkleOff();
  rainOff();
  memset(collarNow, 0, sizeof(collarNow));
  collarOpen = false;
  memset(shoulderGlow, 0, sizeof(shoulderGlow));
  pedalDown = false;
  for (Ripple& ripple : ripples) ripple.active = false;
  for (Runner& runner : runners) runner.active = false;
  for (Runner& runner : shoulderRunners) runner.active = false;
}

void capeShow(LinkState state) {
  static uint32_t lastFrameMs = 0;
  const uint32_t now = millis();
  if (now - lastFrameMs < FRAME_MS) return;
  if (lastFrameMs != 0 && now - lastFrameMs > longestGapMs) longestGapMs = now - lastFrameMs;
  lastFrameMs = now;

  if (state == LinkState::Connected && lastState != LinkState::Connected) connectedAtMs = now;
  lastState = state;

  if (state != LinkState::Connected) {
    // Not connected: the hem row blinks blue while searching and holds
    // steady while connecting. Everything else is dark.
    const bool lit = state == LinkState::Connecting || (now / 500) % 2;
    fill_solid(lapelOne, LEDS_PER_LAPEL, CRGB::Black);
    fill_solid(lapelTwo, LEDS_PER_LAPEL, CRGB::Black);
    strandsDark();
    shouldersDark();
    setRow(0, lit ? CRGB(0, 0, 120) : CRGB::Black);
    collarDark();
    if (lit) collarMarks(0, CRGB(0, 0, 120));
  } else {
    if (PRESETS[presetNow].rain) {
      drawRain(now);
    } else {
      updateHueShift(now);
      drawNotes(now);
      drawStrands(now);
      drawShoulders(now);
      drawJoystickSparkle(now);
      drawCollar(now);
    }

    // Just connected: one soft green wavefront climbs the lapels, so the
    // wearer can see the link is up without looking at the box.
    const uint32_t sinceConnect = now - connectedAtMs;
    const uint32_t SWEEP_MS = 600;
    if (sinceConnect < SWEEP_MS) {
      const uint8_t row = (uint8_t)(sinceConnect * (ROWS - 1) / SWEEP_MS);
      setRow(row, CRGB(0, 120, 0));
    } else if (sinceConnect < SWEEP_MS + COLLAR_SWEEP_MS) {
      // ...and carries on round the collar, from both bottoms to the back.
      const uint32_t travel = COLLAR_PIXELS / 2 - COLLAR_MARK_PIXELS;
      collarMarks((uint16_t)((sinceConnect - SWEEP_MS) * travel / COLLAR_SWEEP_MS), CRGB(0, 120, 0));
    }
  }

  drawPresetSignal(now);
  FastLED.show();

  framesThisSecond++;
  if (now - secondStartedMs >= 1000) {
    framesLastSecond = framesThisSecond;
    framesThisSecond = 0;
    secondStartedMs = now;
  }
}
