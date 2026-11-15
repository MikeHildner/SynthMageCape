package com.example.midimonitor.midi

import java.util.UUID

/**
 * The identifiers every BLE MIDI device uses. They are fixed by the BLE MIDI
 * specification, so the same two values go into the ESP32 firmware.
 *
 * In Bluetooth LE a device exposes *services*, each a group of *characteristics*
 * (individual values that can be read, written or subscribed to). BLE MIDI is
 * one service with one characteristic; all MIDI data flows through it.
 */
object BleMidiUuids {
    /** The MIDI service. Devices include this in their advertising, which is how a scan finds them. */
    val SERVICE: UUID = UUID.fromString("03B80E5A-EDE8-4B33-A751-6CE34EC4C700")

    /** The MIDI I/O characteristic: subscribe for incoming packets, write for outgoing ones. */
    val IO_CHARACTERISTIC: UUID = UUID.fromString("7772E5DB-3868-4112-A1A9-F2669D106BF3")
}
