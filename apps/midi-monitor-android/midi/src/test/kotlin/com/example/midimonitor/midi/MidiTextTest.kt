package com.example.midimonitor.midi

import com.example.midimonitor.midi.BleMidiEncoder.bytes
import com.example.midimonitor.midi.MidiMessage.ControlChange
import com.example.midimonitor.midi.MidiMessage.NoteOff
import com.example.midimonitor.midi.MidiMessage.NoteOn
import com.example.midimonitor.midi.MidiMessage.PitchBend
import com.example.midimonitor.midi.MidiMessage.TimingClock
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertNull

class MidiTextTest {

    @Test
    fun `notes are described by name`() {
        assertEquals("Note On   C4  velocity 100", NoteOn(0, 60, 100).describe())
        assertEquals("Note Off  C4  release velocity 64", NoteOff(0, 60, 64).describe())
        assertEquals("Note Off  C4  (sent as Note On, velocity 0)", NoteOn(0, 60, 0).describe())
    }

    @Test
    fun `known controllers are named and pedals show on or off`() {
        assertEquals("CC 64 Sustain pedal  127 (on)", ControlChange(0, 64, 127).describe())
        assertEquals("CC 64 Sustain pedal  0 (off)", ControlChange(0, 64, 0).describe())
        assertEquals("CC 1 Mod wheel  37", ControlChange(0, 1, 37).describe())
        assertEquals("CC 22  5", ControlChange(0, 22, 5).describe())
    }

    @Test
    fun `pitch bend shows its sign`() {
        assertEquals("Pitch bend  +8191", PitchBend(0, 8191).describe())
        assertEquals("Pitch bend  0", PitchBend(0, 0).describe())
        assertEquals("Pitch bend  -8192", PitchBend(0, -8192).describe())
    }

    @Test
    fun `channel number is one-based, and absent for system messages`() {
        assertEquals(1, NoteOn(0, 60, 100).channelNumber)
        assertEquals(16, NoteOn(15, 60, 100).channelNumber)
        assertNull(TimingClock.channelNumber)
    }

    @Test
    fun `system exclusive shows its bytes and names the maker`() {
        assertEquals(
            "System Exclusive (Korg)  4 bytes: 42 30 68 41",
            MidiMessage.SystemExclusive(bytes(0x42, 0x30, 0x68, 0x41)).describe(),
        )
        assertEquals(
            "System Exclusive  2 bytes: 41 10",
            MidiMessage.SystemExclusive(bytes(0x41, 0x10)).describe(),
        )
    }

    @Test
    fun `long system exclusive is cut off after 32 bytes`() {
        val text = MidiMessage.SystemExclusive(ByteArray(40) { 0x01 }).describe()
        assertEquals("System Exclusive  40 bytes: " + List(32) { "01" }.joinToString(" ") + " …", text)
    }

    @Test
    fun `hex string`() {
        assertEquals("80 80 90 3C 7F", bytes(0x80, 0x80, 0x90, 0x3C, 0x7F).toHexString())
    }

    // ---- Encoder ----

    @Test
    fun `encoder splits the timestamp between header and timestamp byte`() {
        // The same packet the parser test decodes: 7421 ms -> header B9, timestamp FD.
        assertContentEquals(
            bytes(0xB9, 0xFD, 0xB0, 0x40, 0x7F),
            BleMidiEncoder.encode(7421, bytes(0xB0, 0x40, 0x7F)),
        )
    }

    @Test
    fun `what the encoder writes, the parser reads back`() {
        val packet = BleMidiEncoder.encode(
            5000,
            bytes(0x90, 60, 100),
            bytes(0xB0, 64, 127),
            bytes(0xE0, 0x00, 0x60),
            bytes(0xF8),
        )
        val result = BleMidiParser().parse(packet)
        assertEquals(emptyList(), result.problems)
        assertEquals(
            listOf(
                TimedMessage(5000, NoteOn(0, 60, 100)),
                TimedMessage(5000, ControlChange(0, 64, 127)),
                TimedMessage(5000, PitchBend(0, 4096)),
                TimedMessage(5000, TimingClock),
            ),
            result.messages,
        )
    }
}
