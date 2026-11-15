package com.example.midimonitor

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.example.midimonitor.ble.BleScanner
import com.example.midimonitor.ble.ScannedDevice
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.Job
import kotlinx.coroutines.cancelAndJoin
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import kotlinx.coroutines.withTimeoutOrNull

/**
 * Everything the scan screen needs in order to draw itself, as one immutable
 * value. The screen never holds scan state of its own; it renders whatever the
 * latest ScanUiState says.
 */
data class ScanUiState(
    val scanning: Boolean = false,
    val midiOnly: Boolean = true,
    val devices: List<ScannedDevice> = emptyList(),
    val error: String? = null,
)

/**
 * Holds the scan state and runs the scan.
 *
 * Why a ViewModel: Android destroys and recreates an Activity when the screen
 * rotates (and at other times). A ViewModel outlives that, so the device list and
 * a running scan survive. It also gives us viewModelScope, a coroutine scope that
 * is cancelled when the screen is closed for good.
 *
 * AndroidViewModel is the variant that is handed the Application, which is needed
 * here to reach the Bluetooth system service.
 */
class ScanViewModel(application: Application) : AndroidViewModel(application) {

    private val scanner = BleScanner(application)

    // The usual pattern: a private mutable flow, exposed read-only.
    private val _state = MutableStateFlow(ScanUiState())
    val state: StateFlow<ScanUiState> = _state.asStateFlow()

    private var scanJob: Job? = null

    val isBluetoothEnabled: Boolean
        get() = scanner.isBluetoothEnabled

    fun startScan() {
        val previous = scanJob
        scanJob = viewModelScope.launch {
            // Let any earlier scan finish stopping first, so two scans never overlap.
            previous?.cancelAndJoin()

            _state.update { it.copy(scanning = true, devices = emptyList(), error = null) }
            try {
                // Scanning costs battery, and Android throttles apps that scan
                // without end, so give up after a while.
                withTimeoutOrNull(SCAN_DURATION_MS) {
                    scanner.scan(_state.value.midiOnly).collect { device ->
                        _state.update { it.copy(devices = it.devices.withDevice(device)) }
                    }
                }
            } catch (e: CancellationException) {
                throw e // cancellation is how stopScan() works; let it through
            } catch (e: Exception) {
                _state.update { it.copy(error = "Scan failed: ${e.message}") }
            } finally {
                _state.update { it.copy(scanning = false) }
            }
        }
    }

    fun stopScan() {
        // Cancelling the coroutine cancels the flow collection, which runs
        // awaitClose in BleScanner, which stops the actual Bluetooth scan.
        scanJob?.cancel()
    }

    fun setMidiOnly(midiOnly: Boolean) {
        val wasScanning = _state.value.scanning
        _state.update { it.copy(midiOnly = midiOnly) }
        if (wasScanning) startScan() // restart with the new filter
    }

    private companion object {
        const val SCAN_DURATION_MS = 30_000L
    }
}

/**
 * Returns the list with [device] added, or replacing the earlier sighting of the
 * same address. Devices keep the order in which they were first seen so the list
 * does not jump around as signal strengths change.
 */
private fun List<ScannedDevice>.withDevice(device: ScannedDevice): List<ScannedDevice> {
    val index = indexOfFirst { it.address == device.address }
    return if (index < 0) this + device else toMutableList().also { it[index] = device }
}
