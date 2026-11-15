package com.example.midimonitor.midi

/**
 * One decoded MIDI 1.0 message.
 *
 * MIDI is a byte stream. A byte with bit 7 set is a *status* byte (what kind of
 * message); a byte with bit 7 clear is a *data* byte (0..127). For channel
 * messages the status byte's high nibble is the message type and the low nibble
 * is the channel:
 *
 *     1001 0010   0x92 = Note On, channel index 2 (shown to musicians as channel 3)
 *
 * Channels are stored here as 0..15, as they are on the wire.
 */
sealed interface MidiMessage {

    /** A message addressed to one of the 16 channels. */
    sealed interface ChannelMessage : MidiMessage {
        val channel: Int
    }

    // ---- Channel voice messages (status 0x80..0xEF) ----

    /** 0x8n: key released. Velocity is how fast it was released; most keyboards send 64 or 0. */
    data class NoteOff(override val channel: Int, val note: Int, val velocity: Int) : ChannelMessage

    /**
     * 0x9n: key pressed. By long-standing MIDI convention a Note On with velocity 0
     * means "note off", which lets a keyboard release keys without changing status
     * byte (see running status in [BleMidiParser]). The message is kept as it was
     * sent so the monitor can show which form the instrument really uses.
     */
    data class NoteOn(override val channel: Int, val note: Int, val velocity: Int) : ChannelMessage {
        val isNoteOff: Boolean get() = velocity == 0
    }

    /** 0xAn: pressure on one held key. */
    data class PolyAftertouch(override val channel: Int, val note: Int, val pressure: Int) : ChannelMessage

    /** 0xBn: a controller moved. Controller 1 is the mod wheel, 64 is the sustain pedal. */
    data class ControlChange(override val channel: Int, val controller: Int, val value: Int) : ChannelMessage

    /** 0xCn: select a sound. One data byte. */
    data class ProgramChange(override val channel: Int, val program: Int) : ChannelMessage

    /** 0xDn: pressure on the keyboard as a whole. One data byte. */
    data class ChannelAftertouch(override val channel: Int, val pressure: Int) : ChannelMessage

    /**
     * 0xEn: pitch wheel. Sent as two 7-bit data bytes, least significant first,
     * forming a 14-bit number 0..16383 with 8192 as centre. [value] is re-centred
     * to -8192..8191 so that 0 means "wheel at rest".
     */
    data class PitchBend(override val channel: Int, val value: Int) : ChannelMessage

    // ---- System common messages (status 0xF0..0xF7) ----

    /**
     * 0xF0 ... 0xF7: manufacturer-specific data of any length. [payload] is the
     * bytes between the 0xF0 and the 0xF7, without either.
     */
    class SystemExclusive(val payload: ByteArray) : MidiMessage {
        // Arrays compare by identity in Kotlin, so equality is spelled out.
        override fun equals(other: Any?) = other is SystemExclusive && payload.contentEquals(other.payload)
        override fun hashCode() = payload.contentHashCode()
        override fun toString() = "SystemExclusive(${payload.joinToString(" ") { "%02X".format(it) }})"
    }

    /** 0xF1: MIDI Time Code quarter frame. */
    data class MtcQuarterFrame(val value: Int) : MidiMessage

    /** 0xF2: song position in MIDI beats (one beat = six clock ticks = a sixteenth note). */
    data class SongPosition(val beats: Int) : MidiMessage

    /** 0xF3: select a song. */
    data class SongSelect(val song: Int) : MidiMessage

    /** 0xF6: ask analogue synths to tune themselves. */
    data object TuneRequest : MidiMessage

    // ---- System real-time messages (status 0xF8..0xFF), one byte each ----

    /** 0xF8: sent 24 times per quarter note while a clock is running. */
    data object TimingClock : MidiMessage

    /** 0xFA */
    data object Start : MidiMessage

    /** 0xFB */
    data object Continue : MidiMessage

    /** 0xFC */
    data object Stop : MidiMessage

    /** 0xFE: "I am still here" keep-alive, roughly every 300 ms on instruments that use it. */
    data object ActiveSensing : MidiMessage

    /** 0xFF */
    data object SystemReset : MidiMessage

    /** A status byte the MIDI 1.0 specification leaves undefined (0xF4, 0xF5, 0xF9, 0xFD). */
    data class Undefined(val status: Int) : MidiMessage

    companion object {
        /**
         * How many data bytes follow [status]. This table is the heart of any MIDI
         * parser: it is the only way to know where one message ends.
         * Returns 0 for single-byte messages, and for 0xF0, whose length is open-ended.
         */
        fun dataLength(status: Int): Int = when (status and 0xF0) {
            0x80, 0x90, 0xA0, 0xB0, 0xE0 -> 2
            0xC0, 0xD0 -> 1
            else -> when (status) {
                0xF1, 0xF3 -> 1
                0xF2 -> 2
                else -> 0
            }
        }

        /**
         * Builds a message from a status byte and up to two data bytes.
         * Not used for System Exclusive, which the parser assembles itself.
         */
        fun decode(status: Int, data1: Int = 0, data2: Int = 0): MidiMessage {
            val channel = status and 0x0F
            return when (status and 0xF0) {
                0x80 -> NoteOff(channel, data1, data2)
                0x90 -> NoteOn(channel, data1, data2)
                0xA0 -> PolyAftertouch(channel, data1, data2)
                0xB0 -> ControlChange(channel, data1, data2)
                0xC0 -> ProgramChange(channel, data1)
                0xD0 -> ChannelAftertouch(channel, data1)
                0xE0 -> PitchBend(channel, ((data2 shl 7) or data1) - 8192)
                else -> when (status) {
                    0xF1 -> MtcQuarterFrame(data1)
                    0xF2 -> SongPosition((data2 shl 7) or data1)
                    0xF3 -> SongSelect(data1)
                    0xF6 -> TuneRequest
                    0xF8 -> TimingClock
                    0xFA -> Start
                    0xFB -> Continue
                    0xFC -> Stop
                    0xFE -> ActiveSensing
                    0xFF -> SystemReset
                    else -> Undefined(status)
                }
            }
        }
    }
}
