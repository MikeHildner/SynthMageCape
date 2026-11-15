package com.example.midimonitor.midi

private val PITCH_CLASSES = arrayOf("C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B")

/**
 * Name of a MIDI note number, for example 60 -> "C4" and 69 -> "A4" (440 Hz).
 *
 * Twelve notes per octave, and note 0 is C-1. Manufacturers disagree about the
 * octave number: this is the convention where middle C (note 60) is C4, which is
 * what Korg uses. Yamaha calls the same key C3.
 */
fun noteName(note: Int): String = PITCH_CLASSES[note % 12] + (note / 12 - 1)
