package com.example.midimonitor.midi

import com.example.midimonitor.midi.MidiMessage.ActiveSensing
import com.example.midimonitor.midi.MidiMessage.ChannelAftertouch
import com.example.midimonitor.midi.MidiMessage.ChannelMessage
import com.example.midimonitor.midi.MidiMessage.Continue
import com.example.midimonitor.midi.MidiMessage.ControlChange
import com.example.midimonitor.midi.MidiMessage.MtcQuarterFrame
import com.example.midimonitor.midi.MidiMessage.NoteOff
import com.example.midimonitor.midi.MidiMessage.NoteOn
import com.example.midimonitor.midi.MidiMessage.PitchBend
import com.example.midimonitor.midi.MidiMessage.PolyAftertouch
import com.example.midimonitor.midi.MidiMessage.ProgramChange
import com.example.midimonitor.midi.MidiMessage.SongPosition
import com.example.midimonitor.midi.MidiMessage.SongSelect
import com.example.midimonitor.midi.MidiMessage.Start
import com.example.midimonitor.midi.MidiMessage.Stop
import com.example.midimonitor.midi.MidiMessage.SystemExclusive
import com.example.midimonitor.midi.MidiMessage.SystemReset
import com.example.midimonitor.midi.MidiMessage.TimingClock
import com.example.midimonitor.midi.MidiMessage.TuneRequest
import com.example.midimonitor.midi.MidiMessage.Undefined

/** "80 80 90 3C 7F": the way packets are written in the tests and shown in the app. */
fun ByteArray.toHexString(): String = joinToString(" ") { "%02X".format(it) }

/** The channel as musicians number it, 1..16, or null for messages that have no channel. */
val MidiMessage.channelNumber: Int?
    get() = (this as? ChannelMessage)?.let { it.channel + 1 }

/**
 * A one-line, human-readable description of the message, without the channel.
 *
 * `when` over a sealed interface is checked by the compiler: add a new message
 * type and this function stops compiling until it is handled here.
 */
fun MidiMessage.describe(): String = when (this) {
    is NoteOn ->
        if (isNoteOff) "Note Off  ${noteName(note)}  (sent as Note On, velocity 0)"
        else "Note On   ${noteName(note)}  velocity $velocity"
    is NoteOff -> "Note Off  ${noteName(note)}  release velocity $velocity"
    is PolyAftertouch -> "Poly aftertouch  ${noteName(note)}  $pressure"
    is ControlChange -> describeControlChange(this)
    is ProgramChange -> "Program change  $program"
    is ChannelAftertouch -> "Channel aftertouch  $pressure"
    is PitchBend -> "Pitch bend  " + if (value > 0) "+$value" else "$value"
    is SystemExclusive -> describeSystemExclusive(this)
    is MtcQuarterFrame -> "MTC quarter frame  $value"
    is SongPosition -> "Song position  $beats"
    is SongSelect -> "Song select  $song"
    TuneRequest -> "Tune request"
    TimingClock -> "Clock tick"
    Start -> "Start"
    Continue -> "Continue"
    Stop -> "Stop"
    ActiveSensing -> "Active sensing"
    SystemReset -> "System reset"
    is Undefined -> "Undefined status %02X".format(status)
}

/**
 * System Exclusive is the escape hatch of MIDI: after F0 comes a manufacturer
 * ID, and everything after that is whatever the manufacturer decided. The bytes
 * are shown in full (between the F0 and F7, which are left out) because their
 * meaning can only be found in the instrument's own MIDI implementation document.
 */
private fun describeSystemExclusive(sysex: SystemExclusive): String {
    val payload = sysex.payload
    val shown = payload.take(SYSEX_BYTES_SHOWN).toByteArray().toHexString()
    val more = if (payload.size > SYSEX_BYTES_SHOWN) " …" else ""
    // The first byte is the manufacturer ID. Only the ones likely to turn up
    // here are named; 7E and 7F are the "universal" messages any maker may use.
    val maker = when (payload.firstOrNull()?.toInt()) {
        0x42 -> " (Korg)"
        0x7E -> " (universal non-real-time)"
        0x7F -> " (universal real-time)"
        else -> ""
    }
    return "System Exclusive$maker  ${payload.size} bytes: $shown$more"
}

private const val SYSEX_BYTES_SHOWN = 32

private fun describeControlChange(cc: ControlChange): String {
    val name = CONTROLLER_NAMES[cc.controller]
    val label = if (name != null) "CC ${cc.controller} $name" else "CC ${cc.controller}"
    // Controllers 64..69 are pedals and switches: the MIDI specification says a
    // value of 64 or more means on. A simple pedal sends only 0 and 127; a
    // half-damper pedal sends the values in between as well.
    val meaning = when {
        cc.controller !in 64..69 -> ""
        cc.value >= 64 -> " (on)"
        else -> " (off)"
    }
    return "$label  ${cc.value}$meaning"
}

/** Names from the MIDI 1.0 controller table, for the controllers a keyboard commonly sends. */
private val CONTROLLER_NAMES = mapOf(
    0 to "Bank select",
    1 to "Mod wheel",
    2 to "Breath",
    4 to "Foot pedal",
    5 to "Portamento time",
    6 to "Data entry",
    7 to "Volume",
    10 to "Pan",
    11 to "Expression",
    32 to "Bank select LSB",
    38 to "Data entry LSB",
    64 to "Sustain pedal",
    65 to "Portamento",
    66 to "Sostenuto pedal",
    67 to "Soft pedal",
    98 to "NRPN LSB",
    99 to "NRPN MSB",
    100 to "RPN LSB",
    101 to "RPN MSB",
    120 to "All sound off",
    121 to "Reset all controllers",
    123 to "All notes off",
)
