package com.example.midimonitor.midi

import kotlin.test.Test
import kotlin.test.assertEquals

class NoteNamesTest {

    @Test
    fun `middle C is C4`() {
        assertEquals("C4", noteName(60))
    }

    @Test
    fun `concert A is A4`() {
        assertEquals("A4", noteName(69))
    }

    @Test
    fun `sharps and the ends of the range`() {
        assertEquals("C-1", noteName(0))
        assertEquals("C#4", noteName(61))
        assertEquals("B3", noteName(59))
        assertEquals("G9", noteName(127))
    }

    @Test
    fun `an 88-key piano runs from A0 to C8`() {
        assertEquals("A0", noteName(21))
        assertEquals("C8", noteName(108))
    }
}
