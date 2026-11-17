#include "ParserSelfTest.h"

#include <Arduino.h>

#include "BleMidiParser.h"
#include "Net.h"

namespace {

struct Expected {
  uint16_t timestamp;
  uint8_t status, data1, data2;
};

struct Case {
  const char* name;
  uint8_t packet[16];
  uint8_t packetLength;
  Expected expected[4];
  uint8_t expectedCount;
  uint8_t expectedProblems;
};

// Taken from BleMidiParserTest.kt in apps/midi-monitor-android.
const Case CASES[] = {
    {"single note on", {0x80, 0x80, 0x90, 0x3C, 0x7F}, 5, {{0, 0x90, 60, 127}}, 1, 0},
    {"status low nibble is the channel", {0x80, 0x80, 0x93, 0x3C, 0x40}, 5, {{0, 0x93, 60, 64}}, 1, 0},
    {"note off, then note on with velocity zero",
     {0x80, 0x80, 0x80, 0x3C, 0x40, 0x80, 0x90, 0x3C, 0x00},
     9,
     {{0, 0x80, 60, 64}, {0, 0x90, 60, 0}},
     2,
     0},
    {"sustain pedal down and up",
     {0x80, 0x80, 0xB0, 0x40, 0x7F, 0x80, 0xB0, 0x40, 0x00},
     9,
     {{0, 0xB0, 64, 127}, {0, 0xB0, 64, 0}},
     2,
     0},
    {"timestamp from header and timestamp byte",
     {0xB9, 0xFD, 0xB0, 0x62, 0x48, 0xFD, 0xB0, 0x06, 0x00, 0xFD, 0xB0, 0x26, 0x0A},
     13,
     {{7421, 0xB0, 98, 72}, {7421, 0xB0, 6, 0}, {7421, 0xB0, 38, 10}},
     3,
     0},
    {"low timestamp bits wrap inside a packet",
     {0x80, 0xFF, 0x90, 0x3C, 0x7F, 0x81, 0x90, 0x3E, 0x7F},
     9,
     {{127, 0x90, 60, 127}, {129, 0x90, 62, 127}},
     2,
     0},
    {"running status",
     {0x80, 0x80, 0x90, 0x3C, 0x7F, 0x40, 0x7F, 0x43, 0x7F},
     9,
     {{0, 0x90, 60, 127}, {0, 0x90, 64, 127}, {0, 0x90, 67, 127}},
     3,
     0},
    {"running status with its own timestamp",
     {0x80, 0x80, 0x90, 0x3C, 0x7F, 0x85, 0x40, 0x7F},
     8,
     {{0, 0x90, 60, 127}, {5, 0x90, 64, 127}},
     2,
     0},
    {"real-time byte does not cancel running status",
     {0x80, 0x80, 0x90, 0x3C, 0x7F, 0x80, 0xF8, 0x40, 0x7F},
     9,
     {{0, 0x90, 60, 127}, {0, 0xF8, 0, 0}, {0, 0x90, 64, 127}},
     3,
     0},
    {"mixed packet from a real device",
     {0xAF, 0xE0, 0x90, 0x3C, 0x7F, 0x40, 0x7F, 0xE0, 0xFB, 0xE0, 0x90, 0x43, 0x7F},
     13,
     {{6112, 0x90, 60, 127}, {6112, 0x90, 64, 127}, {6112, 0xFB, 0, 0}, {6112, 0x90, 67, 127}},
     4,
     0},
    {"system exclusive in one packet",
     {0x80, 0x80, 0xF0, 0x7E, 0x7F, 0x06, 0x01, 0x80, 0xF7},
     9,
     {{0, 0xF0, 0, 0}},
     1,
     0},
    {"message missing a data byte", {0x80, 0x80, 0x90, 0x3C}, 4, {}, 0, 1},
    {"truncated message does not swallow the next one",
     {0x80, 0x80, 0x90, 0x3C, 0x80, 0x90, 0x40, 0x7F},
     8,
     {{0, 0x90, 64, 127}},
     1,
     1},
    {"first byte is not a header", {0x3C, 0x80, 0x90, 0x3C, 0x7F}, 5, {}, 0, 1},
};

struct Collected {
  MidiMessage messages[8];
  uint8_t count = 0;
};

void collect(const MidiMessage& message, void* context) {
  auto* c = static_cast<Collected*>(context);
  if (c->count < 8) c->messages[c->count] = message;
  c->count++;
}

}  // namespace

bool runParserSelfTest() {
  uint8_t passed = 0;
  const uint8_t total = sizeof(CASES) / sizeof(CASES[0]);

  for (const Case& test : CASES) {
    BleMidiParser parser;
    Collected got;
    const uint8_t problems = parser.parse(test.packet, test.packetLength, collect, &got);

    bool ok = (got.count == test.expectedCount) && (problems == test.expectedProblems);
    for (uint8_t k = 0; ok && k < test.expectedCount; k++) {
      const MidiMessage& m = got.messages[k];
      const Expected& e = test.expected[k];
      ok = m.timestamp == e.timestamp && m.status == e.status && m.data1 == e.data1 && m.data2 == e.data2;
    }

    if (ok) {
      passed++;
    } else {
      say("  FAILED: %s (got %u messages, %u problems)\n", test.name, got.count, problems);
    }
  }

  say("Parser self-test: %u of %u passed\n", passed, total);
  return passed == total;
}
