package com.example.midimonitor

import android.app.Application
import android.os.SystemClock
import android.util.Log
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.example.midimonitor.ble.BleMidiConnection
import com.example.midimonitor.ble.BleMidiEvent
import com.example.midimonitor.ble.ConnectionState
import com.example.midimonitor.ble.ScannedDevice
import com.example.midimonitor.midi.BleMidiParser
import com.example.midimonitor.midi.HeldNotes
import com.example.midimonitor.midi.MidiMessage
import com.example.midimonitor.midi.channelNumber
import com.example.midimonitor.midi.describe
import com.example.midimonitor.midi.toHexString
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch

/** One decoded message, ready to display. */
data class MessageLine(
    /** 1..16, or null for system messages. */
    val channel: Int?,
    val text: String,
    /** The sender's 13-bit millisecond timestamp. */
    val timestamp: Int,
)

/** One received packet: the raw bytes and what the parser made of them. */
data class PacketEntry(
    val id: Long,
    /** Milliseconds since the first packet, by the phone's clock. */
    val elapsedMs: Long,
    val hex: String,
    val lines: List<MessageLine>,
    val problems: List<String>,
)

data class MonitorUiState(
    /** Where the packets are coming from, for the screen title. */
    val source: String = "",
    /** One line on how the connection is doing. */
    val status: String = "",
    /** True when [status] describes something that went wrong. */
    val statusIsError: Boolean = false,
    /** Newest first, capped at [MonitorViewModel.MAX_ENTRIES]. */
    val entries: List<PacketEntry> = emptyList(),
    val packetCount: Int = 0,
    val messageCount: Int = 0,
    val clockTicks: Int = 0,
    val problemCount: Int = 0,
    /** Which notes are sounding right now. Always live, even while the list is paused. */
    val heldNotes: HeldNotes = HeldNotes(),
    val paused: Boolean = false,
    val showHex: Boolean = true,
)

/**
 * Turns a stream of BLE MIDI packets into the state of the monitor screen.
 *
 * Packets come from one of two places, the demo or a real Bluetooth connection,
 * and both end up in the same [onPacket].
 */
class MonitorViewModel(application: Application) : AndroidViewModel(application) {

    private val connection = BleMidiConnection(application)
    private val parser = BleMidiParser()

    private val _state = MutableStateFlow(MonitorUiState())
    val state: StateFlow<MonitorUiState> = _state.asStateFlow()

    private var sourceJob: Job? = null
    private var nextId = 0L
    private var firstPacketAt = 0L

    fun startDemo() {
        begin(source = "Demo data", status = "Simulated packets")
        sourceJob = viewModelScope.launch {
            demoPackets().collect { onPacket(it) }
        }
    }

    fun connect(device: ScannedDevice) {
        begin(source = device.name ?: device.address, status = "Connecting")

        // viewModelScope runs on the main thread. The Bluetooth callbacks happen
        // on another thread, but collecting the Flow here brings every event
        // across, so onPacket never runs concurrently with itself and the parser
        // needs no locking.
        sourceJob = viewModelScope.launch {
            connection.connect(device.device).collect { event ->
                when (event) {
                    is BleMidiEvent.PacketReceived -> onPacket(event.packet)
                    is BleMidiEvent.StateChanged -> onConnectionState(event.state, event.detail)
                }
            }
        }
    }

    /** Stops the demo, or disconnects from the device. */
    fun stop() {
        // Cancelling the collector runs awaitClose in BleMidiConnection, which disconnects.
        sourceJob?.cancel()
    }

    private fun begin(source: String, status: String) {
        sourceJob?.cancel()
        parser.reset()
        firstPacketAt = 0L
        _state.value = MonitorUiState(source = source, status = status, showHex = _state.value.showHex)
    }

    private fun onConnectionState(state: ConnectionState, detail: String) {
        val text = when (state) {
            ConnectionState.Connecting -> "Connecting"
            ConnectionState.DiscoveringServices -> "Connected, discovering services"
            ConnectionState.InitialRead -> "MIDI service found, reading"
            ConnectionState.Subscribing -> "Subscribing"
            ConnectionState.Listening -> "Listening"
            ConnectionState.Disconnected -> "Disconnected"
            ConnectionState.Failed -> "Failed"
        }
        _state.update {
            it.copy(
                status = if (detail.isEmpty()) text else "$text: $detail",
                statusIsError = state == ConnectionState.Disconnected || state == ConnectionState.Failed,
            )
        }
    }

    private fun onPacket(packet: ByteArray) {
        // elapsedRealtime is a clock that only moves forward, unlike wall-clock
        // time, which jumps when the phone syncs with the network.
        val now = SystemClock.elapsedRealtime()
        if (firstPacketAt == 0L) firstPacketAt = now

        val parsed = parser.parse(packet)

        // Every packet also goes to the system log, clock ticks included, so a
        // session can be captured from a PC:  adb logcat -s MidiMonitor
        Log.d(LOG_TAG, packet.toHexString())

        // Clock ticks arrive 24 times per quarter note and active sensing three
        // times a second: counted, never listed.
        val clockTicks = parsed.messages.count { it.message == MidiMessage.TimingClock }
        val messageLines = parsed.messages
            .filter { it.message != MidiMessage.TimingClock && it.message != MidiMessage.ActiveSensing }
            .map { MessageLine(it.message.channelNumber, it.message.describe(), it.timestamp) }

        // A packet can carry part of a System Exclusive and complete nothing.
        // Say so, or the packet would be missing from the list and the hex of a
        // long message could never be read in full.
        val pending = parser.pendingSysexSize
        val lines = if (pending != null) {
            messageLines + MessageLine(null, "System Exclusive unfinished, $pending bytes so far", 0)
        } else {
            messageLines
        }

        val entry = if (lines.isEmpty() && parsed.problems.isEmpty()) {
            null // nothing but clock or active sensing in this packet
        } else {
            PacketEntry(nextId++, now - firstPacketAt, packet.toHexString(), lines, parsed.problems)
        }

        _state.update { state ->
            state.copy(
                entries = if (entry == null || state.paused) state.entries else (listOf(entry) + state.entries).take(MAX_ENTRIES),
                packetCount = state.packetCount + 1,
                messageCount = state.messageCount + parsed.messages.size,
                clockTicks = state.clockTicks + clockTicks,
                problemCount = state.problemCount + parsed.problems.size,
                // Feed every message, in order, through the held-notes rules.
                heldNotes = parsed.messages.fold(state.heldNotes) { held, timed -> held.after(timed.message) },
            )
        }
    }

    /** While paused the counters keep running but the list stands still, so it can be read. */
    fun setPaused(paused: Boolean) = _state.update { it.copy(paused = paused) }

    fun setShowHex(showHex: Boolean) = _state.update { it.copy(showHex = showHex) }

    /**
     * Empties the list and the counters. Held notes are reset too: if a note off
     * was ever missed, this is how to get rid of the stuck note.
     */
    fun clear() = _state.update {
        it.copy(
            entries = emptyList(),
            packetCount = 0,
            messageCount = 0,
            clockTicks = 0,
            problemCount = 0,
            heldNotes = HeldNotes(),
        )
    }

    companion object {
        const val MAX_ENTRIES = 500
        private const val LOG_TAG = "MidiMonitor"
    }
}
