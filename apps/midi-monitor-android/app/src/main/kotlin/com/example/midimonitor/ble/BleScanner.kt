package com.example.midimonitor.ble

import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothManager
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.os.ParcelUuid
import com.example.midimonitor.midi.BleMidiUuids
import kotlinx.coroutines.channels.awaitClose
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.callbackFlow

/** One device seen during a scan. */
data class ScannedDevice(
    /** Android's handle for the remote device; this is what a connection is made from. */
    val device: BluetoothDevice,
    /** The device's Bluetooth address, e.g. "AA:BB:CC:DD:EE:FF". Unique, so it is the list key. */
    val address: String,
    /** The name from the advertising data; null if the device does not advertise one. */
    val name: String?,
    /** Received signal strength in dBm. Closer to 0 is stronger: -50 is near, -90 is far. */
    val rssi: Int,
    /** True if the advertising data lists the BLE MIDI service. */
    val advertisesMidi: Boolean,
    /**
     * False for devices that only broadcast (beacons, trackers, many phones):
     * their advertising says "do not connect to me", and an attempt times out.
     */
    val connectable: Boolean,
)

class ScanFailedException(val errorCode: Int) : Exception(
    when (errorCode) {
        ScanCallback.SCAN_FAILED_ALREADY_STARTED -> "a scan is already running"
        ScanCallback.SCAN_FAILED_APPLICATION_REGISTRATION_FAILED -> "the app could not register with the Bluetooth stack"
        ScanCallback.SCAN_FAILED_INTERNAL_ERROR -> "internal Bluetooth error"
        ScanCallback.SCAN_FAILED_FEATURE_UNSUPPORTED -> "this phone does not support the requested scan"
        ScanCallback.SCAN_FAILED_SCANNING_TOO_FREQUENTLY -> "scanning too often; wait half a minute"
        else -> "error code $errorCode"
    },
)

/**
 * Finds Bluetooth LE devices.
 *
 * How discovery works: a BLE peripheral that wants to be found broadcasts small
 * *advertising* packets a few times a second. Scanning means listening for them.
 * Nothing is connected yet; the phone is only overhearing broadcasts.
 */
class BleScanner(context: Context) {

    // BluetoothManager is a system service; the adapter represents the phone's
    // Bluetooth radio. It is null on a device with no Bluetooth at all.
    private val adapter: BluetoothAdapter? =
        context.getSystemService(BluetoothManager::class.java)?.adapter

    val isBluetoothEnabled: Boolean
        get() = adapter?.isEnabled == true

    /**
     * Scans until the collector stops collecting.
     *
     * Android reports scan results through a callback object. callbackFlow adapts
     * that to a Flow: each result is pushed in with trySend, and awaitClose runs
     * when the collecting coroutine is cancelled, which is where the scan is
     * stopped. The caller therefore cannot forget to stop scanning.
     *
     * The same device is emitted many times, once per advertising packet heard,
     * each with a fresh signal strength.
     *
     * Lint cannot see that the UI only reaches this code after the permission
     * has been granted, hence the suppression.
     */
    @SuppressLint("MissingPermission")
    fun scan(midiOnly: Boolean): Flow<ScannedDevice> = callbackFlow {
        val scanner = adapter?.bluetoothLeScanner
        if (scanner == null) {
            // bluetoothLeScanner is null while Bluetooth is switched off.
            close(IllegalStateException("Bluetooth is off"))
            return@callbackFlow
        }

        val midiService = ParcelUuid(BleMidiUuids.SERVICE)

        val callback = object : ScanCallback() {
            override fun onScanResult(callbackType: Int, result: ScanResult) {
                trySend(
                    ScannedDevice(
                        device = result.device,
                        address = result.device.address,
                        name = result.scanRecord?.deviceName,
                        rssi = result.rssi,
                        advertisesMidi = result.scanRecord?.serviceUuids?.contains(midiService) == true,
                        connectable = result.isConnectable,
                    ),
                )
            }

            override fun onScanFailed(errorCode: Int) {
                close(ScanFailedException(errorCode))
            }
        }

        // A filter makes the Bluetooth controller itself discard advertising that
        // does not list the MIDI service. null means "report everything".
        val filters = if (midiOnly) {
            listOf(ScanFilter.Builder().setServiceUuid(midiService).build())
        } else {
            null
        }

        // LOW_LATENCY listens almost continuously: fastest discovery, most battery.
        // That is the right trade for a scan the user is watching.
        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .build()

        scanner.startScan(filters, settings, callback)

        awaitClose {
            // stopScan throws if Bluetooth was switched off mid-scan; nothing to stop then.
            runCatching { scanner.stopScan(callback) }
        }
    }
}
