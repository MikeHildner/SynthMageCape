package com.example.midimonitor.midi

/**
 * Builds BLE MIDI packets: the reverse of [BleMidiParser].
 *
 * The app only listens, so this exists for two reasons: to produce the demo data,
 * and to test the parser by round trip (encode a message, parse it, expect the
 * same message back).
 */
object BleMidiEncoder {

    /**
     * One packet carrying [messages], each a complete MIDI message starting with
     * its status byte, all stamped with the same [timestampMs].
     *
     *     encode(7421, [B0 40 7F])  ->  B9 FD B0 40 7F
     */
    fun encode(timestampMs: Int, vararg messages: ByteArray): ByteArray {
        val timestamp = timestampMs and 0x1FFF                      // keep 13 bits
        val header = (0x80 or (timestamp shr 7)).toByte()           // 1 0 h h h h h h
        val timestampByte = (0x80 or (timestamp and 0x7F)).toByte() // 1 l l l l l l l

        var packet = byteArrayOf(header)
        for (message in messages) {
            packet += timestampByte
            packet += message
        }
        return packet
    }

    /** Convenience for writing a MIDI message as numbers: `bytes(0x90, 60, 100)`. */
    fun bytes(vararg values: Int): ByteArray = ByteArray(values.size) { values[it].toByte() }
}
