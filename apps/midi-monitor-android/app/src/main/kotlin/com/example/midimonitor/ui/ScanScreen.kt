package com.example.midimonitor.ui

import android.app.Activity
import android.bluetooth.BluetoothAdapter
import android.content.Intent
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.Button
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.ListItem
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewmodel.compose.viewModel
import com.example.midimonitor.ScanUiState
import com.example.midimonitor.ScanViewModel
import com.example.midimonitor.ble.ScannedDevice

/**
 * The "stateful" half of the screen: it knows about the ViewModel and Android.
 * It reads the state, and turns button presses into ViewModel calls.
 */
@Composable
fun ScanScreen(
    onDeviceClick: (ScannedDevice) -> Unit,
    onDemoClick: () -> Unit,
    modifier: Modifier = Modifier,
    viewModel: ScanViewModel = viewModel(),
) {
    // Subscribes to the ViewModel's StateFlow. Every new ScanUiState causes the
    // composables that read `state` to run again with the new value.
    val state by viewModel.state.collectAsStateWithLifecycle()

    // The system "Turn on Bluetooth?" dialog, started when Scan is pressed while
    // Bluetooth is off. If the user agrees, carry on with the scan.
    val enableBluetooth = rememberLauncherForActivityResult(
        ActivityResultContracts.StartActivityForResult(),
    ) { result ->
        if (result.resultCode == Activity.RESULT_OK) viewModel.startScan()
    }

    ScanContent(
        state = state,
        onScanClick = {
            when {
                state.scanning -> viewModel.stopScan()
                !viewModel.isBluetoothEnabled -> enableBluetooth.launch(Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE))
                else -> viewModel.startScan()
            }
        },
        onMidiOnlyChange = viewModel::setMidiOnly,
        onDeviceClick = { device ->
            // Stop scanning before connecting: the radio cannot do both well at
            // once, and a connection attempt during a scan is more likely to fail.
            viewModel.stopScan()
            onDeviceClick(device)
        },
        onDemoClick = onDemoClick,
        modifier = modifier,
    )
}

/**
 * The "stateless" half: state comes in as a parameter and events go out through
 * lambdas. It has no idea where either comes from, which keeps it easy to read,
 * preview and test. This split is called state hoisting.
 */
@Composable
private fun ScanContent(
    state: ScanUiState,
    onScanClick: () -> Unit,
    onMidiOnlyChange: (Boolean) -> Unit,
    onDeviceClick: (ScannedDevice) -> Unit,
    onDemoClick: () -> Unit,
    modifier: Modifier = Modifier,
) {
    Column(
        modifier = modifier
            .fillMaxSize()
            .padding(horizontal = 16.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text("MIDI Monitor", style = MaterialTheme.typography.headlineSmall)
            Spacer(Modifier.weight(1f))
            // Opens the monitor screen with simulated packets, no device needed.
            TextButton(onClick = onDemoClick) { Text("Demo") }
        }

        Row(verticalAlignment = Alignment.CenterVertically) {
            Button(onClick = onScanClick) {
                Text(if (state.scanning) "Stop" else "Scan")
            }
            // A weighted Spacer takes all the leftover width, pushing the switch to the right edge.
            Spacer(Modifier.weight(1f))
            Text("BLE MIDI only", modifier = Modifier.padding(end = 8.dp))
            Switch(checked = state.midiOnly, onCheckedChange = onMidiOnlyChange)
        }

        if (state.scanning) {
            LinearProgressIndicator(Modifier.fillMaxWidth())
        }

        if (state.error != null) {
            Text(state.error, color = MaterialTheme.colorScheme.error)
        }

        Text(
            text = when {
                state.devices.isNotEmpty() -> "${state.devices.size} found. Tap one to connect."
                state.scanning -> "Listening for devices"
                else -> "Press Scan to look for devices"
            },
            style = MaterialTheme.typography.labelLarge,
        )

        // LazyColumn only composes the rows that are on screen, like RecyclerView.
        // The key tells Compose which row is which when the list changes.
        LazyColumn(Modifier.fillMaxSize()) {
            items(state.devices, key = { it.address }) { device ->
                DeviceRow(device, onClick = { onDeviceClick(device) })
                HorizontalDivider()
            }
        }
    }
}

@Composable
private fun DeviceRow(device: ScannedDevice, onClick: () -> Unit) {
    ListItem(
        // Modifiers are how behaviour is attached in Compose: clickable makes
        // the whole row respond to taps, with the usual ripple.
        modifier = Modifier.clickable(onClick = onClick),
        headlineContent = { Text(device.name ?: "(no name)") },
        supportingContent = {
            Text(
                buildString {
                    append(device.address)
                    if (device.advertisesMidi) append("  ·  BLE MIDI")
                    if (!device.connectable) append("  ·  not connectable")
                },
            )
        },
        trailingContent = { Text("${device.rssi} dBm") },
    )
}
