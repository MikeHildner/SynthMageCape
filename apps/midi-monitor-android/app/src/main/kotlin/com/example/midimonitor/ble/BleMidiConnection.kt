package com.example.midimonitor.ble

import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothProfile
import android.bluetooth.BluetoothStatusCodes
import android.content.Context
import com.example.midimonitor.midi.BleMidiUuids
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.channels.awaitClose
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.buffer
import kotlinx.coroutines.flow.callbackFlow
import kotlinx.coroutines.launch
import java.util.UUID
import java.util.concurrent.atomic.AtomicReference

enum class ConnectionState { Connecting, DiscoveringServices, InitialRead, Subscribing, Listening, Disconnected, Failed }

/** What a connection reports: progress through the steps below, and then packets. */
sealed interface BleMidiEvent {
    data class StateChanged(val state: ConnectionState, val detail: String = "") : BleMidiEvent

    /** The value of one notification from the MIDI characteristic: one BLE MIDI packet. */
    class PacketReceived(val packet: ByteArray) : BleMidiEvent
}

/**
 * Connects to a BLE MIDI device and delivers the packets it sends.
 *
 * GATT (Generic Attribute Profile) is how connected BLE devices exchange data.
 * The peripheral holds a small database: *services*, each containing
 * *characteristics* (values), each of which may have *descriptors* (settings for
 * that value). The phone is the client and does everything by asking.
 *
 * Receiving MIDI takes five steps, and each one finishes in a callback, not as a
 * return value:
 *
 *  1. connectGatt               -> onConnectionStateChange
 *  2. discoverServices          -> onServicesDiscovered   (read the database layout)
 *  3. read the characteristic   -> onCharacteristicRead   (announce a MIDI client)
 *  4. write the CCCD descriptor -> onDescriptorWrite      (ask to be sent notifications)
 *  5. nothing more to ask       -> onCharacteristicChanged, once per packet
 *
 * A firmware BLE MIDI client (the ESP32, connecting to the WIDI Master) performs
 * the same five steps with the same UUIDs.
 */
class BleMidiConnection(private val context: Context) {

    /**
     * Connects to [device] and stays connected for as long as the Flow is being
     * collected. Cancelling the collector disconnects. The Flow ends by itself if
     * the device disconnects or a step fails.
     */
    @SuppressLint("MissingPermission") // the UI reaches this only after the permission is granted
    fun connect(device: BluetoothDevice): Flow<BleMidiEvent> = callbackFlow {

        // The step currently in progress. Written from the Bluetooth thread and
        // read from the watchdog below, hence an atomic holder, not a plain var.
        val currentStep = AtomicReference(ConnectionState.Connecting)

        fun report(state: ConnectionState, detail: String = "") {
            currentStep.set(state)
            trySend(BleMidiEvent.StateChanged(state, detail))
        }

        // Reports the failure and ends the Flow, which runs awaitClose below.
        fun fail(detail: String) {
            report(ConnectionState.Failed, detail)
            close()
        }

        // Every step waits for a callback, and nothing guarantees the callback
        // ever comes. (Seen in testing: a device that demands pairing and then
        // refuses it leaves the subscription unanswered for good.) So give the
        // whole setup a deadline. This coroutine is cancelled once packets can flow.
        val watchdog = launch {
            delay(SETUP_TIMEOUT_MS)
            val hint = when (currentStep.get()) {
                ConnectionState.Subscribing -> "subscribing was never confirmed; the device may require pairing"
                ConnectionState.DiscoveringServices -> "service discovery never finished"
                ConnectionState.InitialRead -> "the initial read was never answered; the device may require pairing"
                else -> "the device did not answer"
            }
            fail("timed out: $hint")
        }

        // These callbacks run on a Bluetooth system thread, not the main thread.
        // trySend is safe to call from any thread; the collector receives the
        // events on its own thread.
        val callback = object : BluetoothGattCallback() {

            override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
                if (status == BluetoothGatt.GATT_SUCCESS && newState == BluetoothProfile.STATE_CONNECTED) {
                    // Step 2. Connected, but the phone does not yet know what the device offers.
                    report(ConnectionState.DiscoveringServices)
                    gatt.discoverServices()
                } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                    // status carries the reason. Android documents only a few of
                    // the possible values; the rest are shown as a number.
                    val reason = when (status) {
                        BluetoothGatt.GATT_SUCCESS -> ""
                        BluetoothGatt.GATT_CONNECTION_TIMEOUT ->
                            "timed out (status $status): out of range, or the device does not accept connections"
                        else -> "status $status"
                    }
                    report(ConnectionState.Disconnected, reason)
                    close()
                }
            }

            override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
                if (status != BluetoothGatt.GATT_SUCCESS) {
                    fail("service discovery failed, status $status")
                    return
                }

                val characteristic = gatt.getService(BleMidiUuids.SERVICE)
                    ?.getCharacteristic(BleMidiUuids.IO_CHARACTERISTIC)
                if (characteristic == null) {
                    fail("no BLE MIDI service among the ${gatt.services.size} services this device offers")
                    return
                }

                // Ask for a short connection interval. Two connected BLE devices
                // only talk at fixed intervals; HIGH priority requests roughly
                // 11-15 ms, against a default of roughly 30-50 ms. The device may
                // refuse. This is the main dial for latency.
                gatt.requestConnectionPriority(BluetoothGatt.CONNECTION_PRIORITY_HIGH)

                // Step 3. Read the characteristic once before subscribing.
                // Android's own BLE MIDI code does this with the comment
                // "Specification says to read the characteristic first and then
                // switch to receiving notifications". The value that comes back
                // is of no interest (an empty packet); the read itself is what
                // tells the device a MIDI client has arrived.
                report(ConnectionState.InitialRead)
                if (!gatt.readCharacteristic(characteristic)) {
                    // The request could not even be queued. Subscribe anyway.
                    subscribe(gatt, characteristic)
                }
            }

            override fun onCharacteristicRead(
                gatt: BluetoothGatt,
                characteristic: BluetoothGattCharacteristic,
                value: ByteArray,
                status: Int,
            ) {
                // GATT handles one request at a time: the next one may only be
                // sent after the previous one's callback. So the subscription
                // starts here, and it starts whether or not the read succeeded,
                // since some devices may not allow the read at all.
                subscribe(gatt, characteristic)
            }

            private fun subscribe(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
                // Step 4, in two halves. This first half is local: it tells
                // Android to route this characteristic's notifications to us.
                gatt.setCharacteristicNotification(characteristic, true)

                // The second half goes over the air: writing "notifications on"
                // to the Client Characteristic Configuration Descriptor (CCCD)
                // tells the device to start sending. Forgetting this write is the
                // classic reason for "connected but nothing arrives".
                val cccd = characteristic.getDescriptor(CLIENT_CHARACTERISTIC_CONFIG)
                if (cccd == null) {
                    fail("the MIDI characteristic cannot send notifications")
                    return
                }
                report(ConnectionState.Subscribing)
                // The return value only says whether the request was queued;
                // the device's answer arrives in onDescriptorWrite.
                val queued = gatt.writeDescriptor(cccd, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE)
                if (queued != BluetoothStatusCodes.SUCCESS) {
                    fail("could not send the subscription request, code $queued")
                }
            }

            override fun onDescriptorWrite(gatt: BluetoothGatt, descriptor: BluetoothGattDescriptor, status: Int) {
                if (status == BluetoothGatt.GATT_SUCCESS) {
                    watchdog.cancel()
                    report(ConnectionState.Listening)
                } else {
                    fail("could not subscribe to notifications, status $status")
                }
            }

            // Step 5. Called once for every packet the device sends.
            override fun onCharacteristicChanged(
                gatt: BluetoothGatt,
                characteristic: BluetoothGattCharacteristic,
                value: ByteArray,
            ) {
                if (characteristic.uuid == BleMidiUuids.IO_CHARACTERISTIC) {
                    trySend(BleMidiEvent.PacketReceived(value))
                }
            }
        }

        // Step 1.
        //  - autoConnect = false: connect now and give up after about 30 s. `true`
        //    would mean "whenever this device next shows up", for background use.
        //  - TRANSPORT_LE: some devices speak both classic Bluetooth and LE; be explicit.
        report(ConnectionState.Connecting)
        val gatt = device.connectGatt(context, false, callback, BluetoothDevice.TRANSPORT_LE)

        awaitClose {
            // Always close(): each BluetoothGatt holds one of a small number of
            // system-wide client slots, and leaking them eventually makes every
            // connection attempt on the phone fail until Bluetooth is restarted.
            gatt.disconnect()
            gatt.close()
        }
    }
        // By default a callbackFlow holds 64 undelivered events and trySend drops
        // anything beyond that. A MIDI monitor must not lose packets, so let the
        // queue grow instead.
        .buffer(Channel.UNLIMITED)

    private companion object {
        /** How long connecting, discovering and subscribing may take in total. */
        const val SETUP_TIMEOUT_MS = 15_000L

        /** The CCCD's UUID is fixed by the Bluetooth specification (short form 0x2902). */
        val CLIENT_CHARACTERISTIC_CONFIG: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
    }
}
