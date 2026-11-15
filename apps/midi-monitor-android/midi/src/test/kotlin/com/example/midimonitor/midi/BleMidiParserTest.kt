package com.example.midimonitor.midi

import com.example.midimonitor.midi.MidiMessage.ControlChange
import com.example.midimonitor.midi.MidiMessage.NoteOff
import com.example.midimonitor.midi.MidiMessage.NoteOn
import com.example.midimonitor.midi.MidiMessage.PitchBend
import com.example.midimonitor.midi.MidiMessage.ProgramChange
import com.example.midimonitor.midi.MidiMessage.SystemExclusive
import com.example.midimonitor.midi.MidiMessage.TimingClock
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/**
 * Each test feeds the parser a packet written as hex, exactly as it would appear
 * in a Bluetooth sniffer or in this app's raw view, and checks what comes out.
 */
class BleMidiParserTest {

    /** "80 80 90 3C 7F" -> the five bytes. */
    private fun hex(text: String): ByteArray =
        text.trim().split(" ").map { it.toInt(16).toByte() }.toByteArray()

    /** Parses one packet with a fresh parser and insists there were no problems. */
    private fun parseClean(text: String): List<TimedMessage> {
        val result = BleMidiParser().parse(hex(text))
        assertEquals(emptyList(), result.problems)
        return result.messages
    }

    private fun messagesOf(text: String): List<MidiMessage> = parseClean(text).map { it.message }

    // ---- The basic shape: header, timestamp, message ----

    @Test
    fun `single note on`() {
        assertEquals(
            listOf(TimedMessage(0, NoteOn(channel = 0, note = 60, velocity = 127))),
            parseClean("80 80 90 3C 7F"),
        )
    }

    @Test
    fun `low nibble of the status byte is the channel`() {
        assertEquals(listOf(NoteOn(channel = 3, note = 60, velocity = 64)), messagesOf("80 80 93 3C 40"))
    }

    @Test
    fun `note off, and note on with velocity zero`() {
        val messages = messagesOf("80 80 80 3C 40 80 90 3C 00")
        assertEquals(listOf(NoteOff(0, 60, 64), NoteOn(0, 60, 0)), messages)
        assertTrue((messages[1] as NoteOn).isNoteOff)
    }

    @Test
    fun `sustain pedal is controller 64`() {
        assertEquals(
            listOf(ControlChange(0, controller = 64, value = 127), ControlChange(0, controller = 64, value = 0)),
            messagesOf("80 80 B0 40 7F 80 B0 40 00"),
        )
    }

    @Test
    fun `pitch bend is 14 bits, least significant byte first, centred on zero`() {
        assertEquals(listOf(PitchBend(0, 0)), messagesOf("80 80 E0 00 40"))
        assertEquals(listOf(PitchBend(0, 8191)), messagesOf("80 80 E0 7F 7F"))
        assertEquals(listOf(PitchBend(0, -8192)), messagesOf("80 80 E0 00 00"))
    }

    // ---- Timestamps ----

    @Test
    fun `timestamp combines 6 bits from the header with 7 bits from the timestamp byte`() {
        // Header B9 -> high bits 0x39 = 57. Timestamp FD -> low bits 0x7D = 125.
        // 57 * 128 + 125 = 7421 ms. Three control changes share the same instant.
        val messages = parseClean("B9 FD B0 62 48 FD B0 06 00 FD B0 26 0A")
        assertEquals(listOf(7421, 7421, 7421), messages.map { it.timestamp })
        assertEquals(
            listOf(ControlChange(0, 98, 72), ControlChange(0, 6, 0), ControlChange(0, 38, 10)),
            messages.map { it.message },
        )
    }

    @Test
    fun `low timestamp bits wrapping inside a packet carry into the high bits`() {
        // Low bits go 127 then 1: the second message is 2 ms later, not 126 ms earlier.
        val messages = parseClean("80 FF 90 3C 7F 81 90 3E 7F")
        assertEquals(listOf(127, 129), messages.map { it.timestamp })
    }

    // ---- Running status ----

    @Test
    fun `running status, bare data bytes reuse the previous status`() {
        assertEquals(
            listOf(NoteOn(0, 60, 127), NoteOn(0, 64, 127), NoteOn(0, 67, 127)),
            messagesOf("80 80 90 3C 7F 40 7F 43 7F"),
        )
    }

    @Test
    fun `running status may carry its own timestamp byte`() {
        val messages = parseClean("80 80 90 3C 7F 85 40 7F")
        assertEquals(
            listOf(TimedMessage(0, NoteOn(0, 60, 127)), TimedMessage(5, NoteOn(0, 64, 127))),
            messages,
        )
    }

    @Test
    fun `running status works for one-data-byte messages`() {
        assertEquals(listOf(ProgramChange(0, 5), ProgramChange(0, 6)), messagesOf("80 80 C0 05 06"))
    }

    @Test
    fun `a real-time byte does not cancel running status`() {
        assertEquals(
            listOf(NoteOn(0, 60, 127), TimingClock, NoteOn(0, 64, 127)),
            messagesOf("80 80 90 3C 7F 80 F8 40 7F"),
        )
    }

    @Test
    fun `mixed packet from a real device capture`() {
        // Header AF -> high 0x2F, timestamp E0 -> low 0x60: 47 * 128 + 96 = 6112.
        val messages = parseClean("AF E0 90 3C 7F 40 7F E0 FB E0 90 43 7F")
        assertEquals(
            listOf(NoteOn(0, 60, 127), NoteOn(0, 64, 127), MidiMessage.Continue, NoteOn(0, 67, 127)),
            messages.map { it.message },
        )
        assertTrue(messages.all { it.timestamp == 6112 })
    }

    // ---- System Exclusive ----

    @Test
    fun `system exclusive in one packet`() {
        // F0 7E 7F 06 01 F7 is the universal "identity request".
        assertEquals(
            listOf(SystemExclusive(hex("7E 7F 06 01"))),
            messagesOf("80 80 F0 7E 7F 06 01 80 F7"),
        )
    }

    @Test
    fun `system exclusive continues across packets`() {
        val parser = BleMidiParser()
        // Continuation packets have a header and then go straight into data bytes.
        assertEquals(emptyList(), parser.parse(hex("80 80 F0 01 02 03")).messages)
        assertEquals(emptyList(), parser.parse(hex("80 04 05 06")).messages)
        val last = parser.parse(hex("80 07 80 F7"))
        assertEquals(emptyList(), last.problems)
        assertEquals(listOf(SystemExclusive(hex("01 02 03 04 05 06 07"))), last.messages.map { it.message })
    }

    @Test
    fun `real-time byte inside a system exclusive`() {
        assertEquals(
            listOf(TimingClock, SystemExclusive(hex("01 02"))),
            messagesOf("80 80 F0 01 80 F8 02 80 F7"),
        )
    }

    // ---- Packets that break the rules are reported, not guessed at ----

    @Test
    fun `empty packet`() {
        val result = BleMidiParser().parse(ByteArray(0))
        assertEquals(emptyList(), result.messages)
        assertEquals(1, result.problems.size)
    }

    @Test
    fun `first byte is not a header`() {
        val result = BleMidiParser().parse(hex("3C 80 90 3C 7F"))
        assertEquals(emptyList(), result.messages)
        assertEquals(1, result.problems.size)
    }

    @Test
    fun `packet ends after a timestamp byte`() {
        val result = BleMidiParser().parse(hex("80 80 90 3C 7F 80"))
        assertEquals(listOf<MidiMessage>(NoteOn(0, 60, 127)), result.messages.map { it.message })
        assertEquals(1, result.problems.size)
    }

    @Test
    fun `message missing a data byte`() {
        val result = BleMidiParser().parse(hex("80 80 90 3C"))
        assertEquals(emptyList(), result.messages)
        assertEquals(1, result.problems.size)
    }

    @Test
    fun `truncated message does not swallow the one after it`() {
        val result = BleMidiParser().parse(hex("80 80 90 3C 80 90 40 7F"))
        assertEquals(listOf<MidiMessage>(NoteOn(0, 64, 127)), result.messages.map { it.message })
        assertEquals(1, result.problems.size)
    }

    @Test
    fun `data bytes with no status byte in the packet`() {
        // Running status is not carried from one packet to the next.
        val parser = BleMidiParser()
        parser.parse(hex("80 80 90 3C 7F"))
        val result = parser.parse(hex("80 40 7F"))
        assertEquals(emptyList(), result.messages)
        assertEquals(2, result.problems.size)
    }

    @Test
    fun `end of system exclusive without a start`() {
        val result = BleMidiParser().parse(hex("80 80 F7"))
        assertEquals(emptyList(), result.messages)
        assertEquals(1, result.problems.size)
    }
}
