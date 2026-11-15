package com.example.midimonitor.midi

import com.example.midimonitor.midi.MidiMessage.ControlChange
import com.example.midimonitor.midi.MidiMessage.NoteOff
import com.example.midimonitor.midi.MidiMessage.NoteOn
import com.example.midimonitor.midi.MidiMessage.PitchBend
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertSame
import kotlin.test.assertTrue

class HeldNotesTest {

    /** Plays the messages in order, starting from nothing held. */
    private fun play(vararg messages: MidiMessage): HeldNotes =
        messages.fold(HeldNotes()) { state, message -> state.after(message) }

    private val pedalDown = ControlChange(0, 64, 127)
    private val pedalUp = ControlChange(0, 64, 0)

    @Test
    fun `pressed keys are held, lowest first`() {
        val state = play(NoteOn(0, 67, 100), NoteOn(0, 60, 90), NoteOn(0, 64, 80))
        assertEquals(listOf(60, 64, 67), state.downNotes)
        assertEquals(90, state.down[NoteKey(0, 60)])
    }

    @Test
    fun `note off releases a key`() {
        val state = play(NoteOn(0, 60, 90), NoteOn(0, 64, 80), NoteOff(0, 60, 64))
        assertEquals(listOf(64), state.downNotes)
    }

    @Test
    fun `note on with velocity zero releases a key`() {
        val state = play(NoteOn(0, 60, 90), NoteOn(0, 60, 0))
        assertEquals(emptyList(), state.downNotes)
    }

    @Test
    fun `note off for a key that was never down is ignored`() {
        val before = play(NoteOn(0, 60, 90))
        assertSame(before, before.after(NoteOff(0, 72, 64)))
    }

    @Test
    fun `messages that are not about notes change nothing`() {
        val before = play(NoteOn(0, 60, 90))
        assertSame(before, before.after(PitchBend(0, 1000)))
        assertSame(before, before.after(ControlChange(0, 1, 50)))
    }

    // ---- Sustain pedal ----

    @Test
    fun `a key released with the pedal down keeps ringing`() {
        val state = play(NoteOn(0, 60, 90), pedalDown, NoteOff(0, 60, 64))
        assertTrue(state.pedalDown)
        assertEquals(emptyList(), state.downNotes)
        assertEquals(listOf(60), state.sustainedNotes)
        assertEquals(90, state.sustained[NoteKey(0, 60)]) // keeps the velocity it was struck at
    }

    @Test
    fun `pedal up stops the ringing notes but not the held keys`() {
        val state = play(NoteOn(0, 60, 90), NoteOn(0, 64, 80), pedalDown, NoteOff(0, 60, 64), pedalUp)
        assertFalse(state.pedalDown)
        assertEquals(listOf(64), state.downNotes)
        assertEquals(emptyList(), state.sustainedNotes)
    }

    @Test
    fun `pressing the pedal after releasing a key does not bring the note back`() {
        val state = play(NoteOn(0, 60, 90), NoteOff(0, 60, 64), pedalDown)
        assertEquals(emptyList(), state.sustainedNotes)
    }

    @Test
    fun `striking a ringing note makes it a held key again`() {
        val state = play(NoteOn(0, 60, 90), pedalDown, NoteOff(0, 60, 64), NoteOn(0, 60, 50))
        assertEquals(listOf(60), state.downNotes)
        assertEquals(emptyList(), state.sustainedNotes)
        assertEquals(50, state.down[NoteKey(0, 60)])
    }

    @Test
    fun `pedal counts as down from value 64`() {
        assertFalse(play(ControlChange(0, 64, 63)).pedalDown)
        assertTrue(play(ControlChange(0, 64, 64)).pedalDown)
    }

    // ---- Channels ----

    @Test
    fun `the pedal on one channel does not hold notes on another`() {
        val state = play(NoteOn(0, 60, 90), NoteOn(1, 72, 90), pedalDown, NoteOff(0, 60, 64), NoteOff(1, 72, 64))
        assertEquals(mapOf(NoteKey(0, 60) to 90), state.sustained)
        assertEquals(emptyMap(), state.down)
    }

    // ---- Channel mode messages ----

    @Test
    fun `all notes off releases every key on the channel`() {
        val state = play(NoteOn(0, 60, 90), NoteOn(0, 64, 80), NoteOn(1, 72, 90), ControlChange(0, 123, 0))
        assertEquals(setOf(NoteKey(1, 72)), state.down.keys)
    }

    @Test
    fun `all sound off silences ringing notes too`() {
        val state = play(NoteOn(0, 60, 90), NoteOn(0, 64, 80), pedalDown, NoteOff(0, 60, 64), ControlChange(0, 120, 0))
        assertEquals(emptyMap(), state.down)
        assertEquals(emptyMap(), state.sustained)
    }
}
