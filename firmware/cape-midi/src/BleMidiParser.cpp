#include "BleMidiParser.h"

uint8_t midiDataLength(uint8_t status) {
  switch (status & 0xF0) {
    case 0x80:
    case 0x90:
    case 0xA0:
    case 0xB0:
    case 0xE0:
      return 2;
    case 0xC0:
    case 0xD0:
      return 1;
    default:
      if (status == 0xF1 || status == 0xF3) return 1;
      if (status == 0xF2) return 2;
      return 0;
  }
}

uint8_t BleMidiParser::parse(const uint8_t* packet, size_t length, Handler onMessage, void* context) {
  problems_ = 0;

  if (length == 0) {
    problem("empty packet");
    return problems_;
  }
  const uint8_t header = packet[0];
  if ((header & 0x80) == 0) {
    problem("first byte is not a header (bit 7 is clear)");
    return problems_;
  }

  uint8_t timestampHigh = header & 0x3F;
  uint8_t timestampLow = 0;
  bool seenTimestamp = false;
  uint8_t runningStatus = 0;  // 0 means "none"; a real status byte is never 0
  size_t i = 1;

  while (i < length) {
    uint8_t b = packet[i];

    if (b & 0x80) {
      // Bit 7 set at a message boundary: a timestamp byte.
      const uint8_t low = b & 0x7F;
      // The header's high bits are sent only once. If the low bits go backwards
      // inside a packet, the 7-bit counter overflowed, so carry one into the
      // high bits.
      if (seenTimestamp && low < timestampLow) {
        timestampHigh = (timestampHigh + 1) & 0x3F;
      }
      timestampLow = low;
      seenTimestamp = true;
      i++;

      if (i >= length) {
        problem("packet ends right after a timestamp byte");
        break;
      }

      b = packet[i];
      if (b & 0x80) {
        // Timestamp followed by a status byte: the normal case.
        i++;
        const uint16_t timestamp = (uint16_t)((timestampHigh << 7) | timestampLow);

        if (b >= 0xF8) {
          // Real-time messages are a single byte. They may appear anywhere,
          // even inside a System Exclusive, and do not disturb running status.
          onMessage(MidiMessage{timestamp, b, 0, 0}, context);
          continue;
        }

        if (inSysex_ && b != 0xF7) {
          problem("System Exclusive cut short by another status byte");
          inSysex_ = false;
        }

        if (b == 0xF0) {
          // Start of System Exclusive. Its data bytes are skipped further down.
          inSysex_ = true;
          runningStatus = 0;
        } else if (b == 0xF7) {
          if (!inSysex_) {
            problem("end of System Exclusive (F7) without a start (F0)");
          } else {
            onMessage(MidiMessage{timestamp, 0xF0, 0, 0}, context);
            inSysex_ = false;
          }
          runningStatus = 0;
        } else {
          // Only channel messages (below 0xF0) set running status; system
          // common messages cancel it.
          runningStatus = (b < 0xF0) ? b : 0;
          i = readMessage(b, packet, length, i, timestamp, onMessage, context);
        }
        continue;
      }
      // Timestamp followed by a data byte: running status with a fresh
      // timestamp. Fall through with b as that data byte.
    }

    // b is a data byte at a message boundary.
    if (inSysex_) {
      i++;
    } else if (runningStatus != 0) {
      const uint16_t timestamp = (uint16_t)((timestampHigh << 7) | timestampLow);
      i = readMessage(runningStatus, packet, length, i, timestamp, onMessage, context);
    } else {
      problem("data byte with no status byte before it in this packet");
      i++;
    }
  }

  return problems_;
}

// Reads the data bytes that `status` calls for, starting at `start`, and hands
// over the message. Returns the index of the first byte not consumed.
size_t BleMidiParser::readMessage(uint8_t status, const uint8_t* packet, size_t length, size_t start,
                                  uint16_t timestamp, Handler onMessage, void* context) {
  const uint8_t needed = midiDataLength(status);
  uint8_t data[2] = {0, 0};
  size_t i = start;
  for (uint8_t k = 0; k < needed; k++) {
    if (i >= length || (packet[i] & 0x80)) {
      // Ran out of packet, or hit a timestamp byte, before the message was
      // complete. Leave the offending byte for the main loop.
      problem("message is missing data bytes");
      return i;
    }
    data[k] = packet[i];
    i++;
  }
  onMessage(MidiMessage{timestamp, status, data[0], data[1]}, context);
  return i;
}
