package com.example.midimonitor.midi

/** A MIDI message together with the sender's 13-bit millisecond timestamp (0..8191). */
data class TimedMessage(val timestamp: Int, val message: MidiMessage)

/**
 * The result of parsing one BLE MIDI packet: the messages found, plus a note of
 * anything that did not follow the format. A monitor should show the problems
 * rather than hide them; they are how you find out what a device really does.
 */
data class ParsedPacket(val messages: List<TimedMessage>, val problems: List<String>)

/**
 * Parses BLE MIDI packets: the value of one notification from the MIDI I/O
 * characteristic (7772E5DB-3868-4112-A1A9-F2669D106BF3).
 *
 * A packet is ordinary MIDI with two kinds of extra byte mixed in:
 *
 *     byte 0        header      1 0 h h h h h h   high 6 bits of the timestamp
 *     before each   timestamp   1 l l l l l l l   low 7 bits of the timestamp
 *     status byte
 *
 *     80 80 90 3C 7F          header, timestamp, Note On, note 60, velocity 127
 *
 * The timestamp is the sender's clock in milliseconds, 13 bits wide, so it wraps
 * every 8.192 seconds. The header is sent once per packet; each message has its
 * own timestamp byte.
 *
 * The awkward part: header, timestamp and status bytes all have bit 7 set, so a
 * byte's meaning depends on where it sits. The rule this parser follows is
 * "at a message boundary, a byte with bit 7 set is a timestamp, and the byte
 * after it is a status byte (or a data byte, for running status)".
 *
 * Running status: after a channel message, more messages of the same type and
 * channel may follow as bare data bytes, with no status byte and optionally no
 * timestamp byte:
 *
 *     80 80 90 3C 7F 40 7F    two Note Ons (60 and 64) sharing one status byte
 *
 * This parser applies running status within one packet only, and reports bare
 * data bytes at the start of a packet as a problem. The official specification
 * PDF is restricted to MIDI Association members and could not be consulted, so
 * that choice is the conservative reading; if a real device shows up in the
 * problem list for this reason, the rule should be revisited.
 *
 * System Exclusive is the one message allowed to span packets, so it is the only
 * state kept between calls to [parse]. Use one parser per connection.
 */
class BleMidiParser {

    /** Payload bytes of a System Exclusive message in progress, or null when there is none. */
    private var sysex: MutableList<Byte>? = null

    /** Forget any half-received System Exclusive. Call when the connection drops. */
    fun reset() {
        sysex = null
    }

    fun parse(packet: ByteArray): ParsedPacket {
        val messages = mutableListOf<TimedMessage>()
        val problems = mutableListOf<String>()

        if (packet.isEmpty()) {
            return ParsedPacket(messages, listOf("empty packet"))
        }

        // Kotlin's Byte is signed (-128..127); `and 0xFF` turns it into 0..255.
        val header = packet[0].toInt() and 0xFF
        if (header and 0x80 == 0) {
            return ParsedPacket(messages, listOf("first byte %02X is not a header (bit 7 is clear)".format(header)))
        }

        var timestampHigh = header and 0x3F
        var timestampLow = 0
        var seenTimestamp = false
        var runningStatus = 0 // 0 means "none"; a real status byte is never 0
        var i = 1

        while (i < packet.size) {
            var b = packet[i].toInt() and 0xFF

            if (b and 0x80 != 0) {
                // Bit 7 set at a message boundary: a timestamp byte.
                val low = b and 0x7F
                // The header's high bits are sent only once. If the low bits go
                // backwards inside a packet, the 7-bit counter overflowed, so
                // carry one into the high bits.
                if (seenTimestamp && low < timestampLow) {
                    timestampHigh = (timestampHigh + 1) and 0x3F
                }
                timestampLow = low
                seenTimestamp = true
                i++

                if (i >= packet.size) {
                    problems += "packet ends right after a timestamp byte"
                    break
                }

                b = packet[i].toInt() and 0xFF
                if (b and 0x80 != 0) {
                    // Timestamp followed by a status byte: the normal case.
                    i++
                    val timestamp = (timestampHigh shl 7) or timestampLow

                    if (b >= 0xF8) {
                        // Real-time messages are a single byte. They may appear
                        // anywhere, even inside a System Exclusive, and they do
                        // not disturb running status.
                        messages += TimedMessage(timestamp, MidiMessage.decode(b))
                        continue
                    }

                    if (sysex != null && b != 0xF7) {
                        problems += "System Exclusive cut short by status %02X".format(b)
                        sysex = null
                    }

                    when {
                        b == 0xF0 -> {
                            // Start of System Exclusive. The data bytes that
                            // follow are collected by the branch further down.
                            sysex = mutableListOf()
                            runningStatus = 0
                        }

                        b == 0xF7 -> {
                            val payload = sysex
                            if (payload == null) {
                                problems += "end of System Exclusive (F7) without a start (F0)"
                            } else {
                                messages += TimedMessage(timestamp, MidiMessage.SystemExclusive(payload.toByteArray()))
                                sysex = null
                            }
                            runningStatus = 0
                        }

                        else -> {
                            // Only channel messages (below 0xF0) set running status;
                            // system common messages cancel it.
                            runningStatus = if (b < 0xF0) b else 0
                            i = readMessage(b, packet, i, timestamp, messages, problems)
                        }
                    }
                    continue
                }
                // Timestamp followed by a data byte: running status with a fresh
                // timestamp. Fall through with b as that data byte.
            }

            // b is a data byte at a message boundary.
            val collecting = sysex
            when {
                collecting != null -> {
                    collecting += b.toByte()
                    i++
                }

                runningStatus != 0 -> {
                    val timestamp = (timestampHigh shl 7) or timestampLow
                    i = readMessage(runningStatus, packet, i, timestamp, messages, problems)
                }

                else -> {
                    problems += "data byte %02X with no status byte before it in this packet".format(b)
                    i++
                }
            }
        }

        return ParsedPacket(messages, problems)
    }

    /**
     * Reads the data bytes that [status] calls for, starting at [start], and adds
     * the decoded message. Returns the index of the first byte not consumed.
     */
    private fun readMessage(
        status: Int,
        packet: ByteArray,
        start: Int,
        timestamp: Int,
        messages: MutableList<TimedMessage>,
        problems: MutableList<String>,
    ): Int {
        val needed = MidiMessage.dataLength(status)
        val data = IntArray(2)
        var i = start
        for (k in 0 until needed) {
            val b = if (i < packet.size) packet[i].toInt() and 0xFF else -1
            if (b < 0 || b and 0x80 != 0) {
                // Ran out of packet, or hit a timestamp byte, before the message
                // was complete. Leave the offending byte for the main loop.
                problems += "message with status %02X is missing data bytes".format(status)
                return i
            }
            data[k] = b
            i++
        }
        messages += TimedMessage(timestamp, MidiMessage.decode(status, data[0], data[1]))
        return i
    }
}
