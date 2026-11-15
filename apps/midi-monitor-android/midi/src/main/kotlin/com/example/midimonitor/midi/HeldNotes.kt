package com.example.midimonitor.midi

import com.example.midimonitor.midi.MidiMessage.ControlChange
import com.example.midimonitor.midi.MidiMessage.NoteOff
import com.example.midimonitor.midi.MidiMessage.NoteOn

/** A key is identified by channel and note: the same note on two channels is two keys. */
data class NoteKey(val channel: Int, val note: Int)

/**
 * Which notes are sounding right now, worked out from the messages seen so far.
 *
 * MIDI never says "these keys are down". It only reports changes, so a receiver
 * has to keep its own picture and update it message by message.
 *
 * A note can be sounding for two different reasons, kept apart here because a
 * light show will probably want to treat them differently:
 *
 *  - [down]: the key is physically held.
 *  - [sustained]: the key has been released, but the sustain pedal was down at
 *    that moment, so the note rings on until the pedal comes up.
 *
 * This is an immutable value: [after] returns a new HeldNotes and leaves this one
 * alone. That suits Compose, which redraws when it is handed a different value,
 * and makes the tests simple. The firmware would more likely keep one mutable
 * array of 128 velocities per channel and update it in place; the rules are the same.
 */
data class HeldNotes(
    /** Keys physically held, with the velocity each was struck at. */
    val down: Map<NoteKey, Int> = emptyMap(),
    /** Keys released while the pedal was down, with their original velocity. */
    val sustained: Map<NoteKey, Int> = emptyMap(),
    /** Channels whose sustain pedal is currently down. */
    val pedalChannels: Set<Int> = emptySet(),
) {
    val pedalDown: Boolean get() = pedalChannels.isNotEmpty()

    /** Note numbers of the held keys, lowest first. */
    val downNotes: List<Int> get() = down.keys.map { it.note }.sorted()

    /** Note numbers ringing on the pedal, lowest first. */
    val sustainedNotes: List<Int> get() = sustained.keys.map { it.note }.sorted()

    /** The state after [message] has been received. Messages that do not affect notes return `this`. */
    fun after(message: MidiMessage): HeldNotes = when (message) {
        is NoteOn ->
            if (message.isNoteOff) release(NoteKey(message.channel, message.note))
            else press(NoteKey(message.channel, message.note), message.velocity)

        is NoteOff -> release(NoteKey(message.channel, message.note))

        is ControlChange -> when (message.controller) {
            CC_SUSTAIN ->
                if (message.value >= 64) {
                    copy(pedalChannels = pedalChannels + message.channel)
                } else {
                    // Pedal up: everything it was holding on this channel stops.
                    copy(
                        pedalChannels = pedalChannels - message.channel,
                        sustained = sustained.filterKeys { it.channel != message.channel },
                    )
                }

            // "All notes off": as if every key on the channel were released.
            CC_ALL_NOTES_OFF ->
                down.keys.filter { it.channel == message.channel }.fold(this) { state, key -> state.release(key) }

            // "All sound off": silence the channel at once, pedal or no pedal.
            CC_ALL_SOUND_OFF -> copy(
                down = down.filterKeys { it.channel != message.channel },
                sustained = sustained.filterKeys { it.channel != message.channel },
            )

            else -> this
        }

        else -> this
    }

    private fun press(key: NoteKey, velocity: Int): HeldNotes =
        // Striking a key that is ringing on the pedal makes it a held key again.
        copy(down = down + (key to velocity), sustained = sustained - key)

    private fun release(key: NoteKey): HeldNotes {
        // A note off for a key we never saw go down: ignore it. This happens when
        // the monitor connects while keys are already held.
        val velocity = down[key] ?: return this
        return if (key.channel in pedalChannels) {
            copy(down = down - key, sustained = sustained + (key to velocity))
        } else {
            copy(down = down - key)
        }
    }

    private companion object {
        const val CC_SUSTAIN = 64
        const val CC_ALL_SOUND_OFF = 120
        const val CC_ALL_NOTES_OFF = 123
    }
}
