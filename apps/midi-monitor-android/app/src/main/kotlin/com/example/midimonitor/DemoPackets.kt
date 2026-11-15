package com.example.midimonitor

import com.example.midimonitor.midi.BleMidiEncoder.bytes
import com.example.midimonitor.midi.BleMidiEncoder.encode
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.flow

/**
 * A short scripted performance, as the BLE MIDI packets a keyboard would send.
 *
 * It stands in for a real device so the monitor screen can be built and tried
 * without one. It produces raw packets, not decoded messages, so everything
 * downstream (parser, log, held notes) runs exactly as it will with real data.
 *
 * `flow { }` builds a cold Flow: the block runs from the top each time something
 * collects it, and `emit` hands one packet to the collector.
 */
fun demoPackets(): Flow<ByteArray> = flow {
    var now = 0 // the pretend keyboard's millisecond clock

    // Waits, then sends one packet stamped with the keyboard's clock.
    suspend fun send(afterMs: Int, vararg messages: ByteArray) {
        delay(afterMs.toLong())
        now += afterMs
        emit(encode(now, *messages))
    }

    send(300, bytes(0xC0, 5))                       // program change

    send(400, bytes(0x90, 60, 90))                  // build a C major chord: C4
    send(250, bytes(0x90, 64, 80))                  // E4
    send(250, bytes(0x90, 67, 100))                 // G4

    send(400, bytes(0xB0, 64, 127))                 // sustain pedal down

    // Release all three keys in one packet using running status: one status
    // byte, then three pairs of data bytes. Velocity 0 turns Note On into note off.
    send(400, bytes(0x90, 60, 0, 64, 0, 67, 0))

    for (value in 0..127 step 16) {                 // mod wheel pushed up
        send(40, bytes(0xB0, 1, value))
    }
    send(40, bytes(0xB0, 1, 0))

    for (bend in listOf(0x48, 0x58, 0x68, 0x7F, 0x60, 0x40)) { // pitch wheel up and back to centre
        send(40, bytes(0xE0, 0x00, bend))
    }

    send(300, bytes(0xD0, 70))                      // channel aftertouch
    send(300, bytes(0xB0, 64, 0))                   // sustain pedal up

    // One quarter note of MIDI clock at 120 BPM: 24 ticks, about 21 ms apart.
    // These must not flood the log.
    repeat(24) {
        send(21, bytes(0xF8))
    }

    // A deliberately broken packet: a Note On with its velocity byte missing.
    delay(300)
    emit(bytes(0x80, 0x80, 0x90, 0x3C))
}
