// BLE MIDI packet parser. A C++ port of BleMidiParser.kt from
// apps/midi-monitor-android, which has the full explanation and the unit tests.
//
// A packet is ordinary MIDI with two kinds of extra byte mixed in:
//
//     byte 0        header      1 0 h h h h h h   high 6 bits of the timestamp
//     before each   timestamp   1 l l l l l l l   low 7 bits of the timestamp
//     status byte
//
//     80 80 90 3C 7F          header, timestamp, Note On, note 60, velocity 127
//
// Differences from the Kotlin version, because this runs on a microcontroller:
// messages are handed to a callback instead of collected in a list, and the
// contents of System Exclusive messages are skipped rather than stored.
#pragma once

#include <stddef.h>
#include <stdint.h>

struct MidiMessage {
  uint16_t timestamp;  // sender's clock, milliseconds, 13 bits (wraps every 8.192 s)
  uint8_t status;      // e.g. 0x90 = Note On, channel 1. 0xF0 = a complete System Exclusive.
  uint8_t data1;       // note number, controller number, ...
  uint8_t data2;       // velocity, controller value, ...

  uint8_t type() const { return status < 0xF0 ? (status & 0xF0) : status; }
  uint8_t channel() const { return status & 0x0F; }  // 0-15; shown to people as 1-16
};

// How many data bytes follow a given status byte.
uint8_t midiDataLength(uint8_t status);

class BleMidiParser {
 public:
  using Handler = void (*)(const MidiMessage& message, void* context);

  // Forget any half-received System Exclusive. Call when the connection drops.
  void reset() { inSysex_ = false; }

  // Parses one notification. Calls onMessage for each MIDI message found and
  // returns the number of things in the packet that did not follow the format.
  // lastProblem() describes the most recent one.
  uint8_t parse(const uint8_t* packet, size_t length, Handler onMessage, void* context);

  const char* lastProblem() const { return lastProblem_; }

 private:
  size_t readMessage(uint8_t status, const uint8_t* packet, size_t length, size_t start,
                     uint16_t timestamp, Handler onMessage, void* context);
  void problem(const char* text) {
    lastProblem_ = text;
    problems_++;
  }

  // System Exclusive is the one message allowed to span packets, so this is the
  // only state kept between calls. Use one parser per connection.
  bool inSysex_ = false;
  uint8_t problems_ = 0;
  const char* lastProblem_ = "";
};
