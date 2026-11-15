package com.example.midimonitor.ui

import android.Manifest
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.provider.Settings
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.LifecycleResumeEffect

private val BLUETOOTH_PERMISSIONS = arrayOf(
    Manifest.permission.BLUETOOTH_SCAN,
    Manifest.permission.BLUETOOTH_CONNECT,
)

private fun Context.hasBluetoothPermissions(): Boolean =
    BLUETOOTH_PERMISSIONS.all { checkSelfPermission(it) == PackageManager.PERMISSION_GRANTED }

/**
 * Shows [content] once the Bluetooth permissions are granted, and an explanation
 * with a button until then. Nothing inside [content] can run without permission,
 * which is what makes it safe for the Bluetooth code to assume it has it.
 */
@Composable
fun BluetoothPermissionGate(modifier: Modifier = Modifier, content: @Composable () -> Unit) {
    val context = LocalContext.current

    // `remember` keeps a value across recompositions; `mutableStateOf` makes it
    // observable, so assigning to `granted` redraws whatever reads it. `by` is
    // Kotlin property delegation: read and write `granted` as a plain Boolean.
    var granted by remember { mutableStateOf(context.hasBluetoothPermissions()) }

    // Asking for a permission means starting a system dialog and getting a result
    // back later. This registers the "what to do with the result" half; calling
    // launch() below is the "show the dialog" half.
    val permissionRequest = rememberLauncherForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions(),
    ) { results ->
        granted = results.values.all { it }
    }

    // The user can also change the permission in system Settings and come back,
    // so check again every time the app returns to the foreground.
    LifecycleResumeEffect(Unit) {
        granted = context.hasBluetoothPermissions()
        onPauseOrDispose { }
    }

    if (granted) {
        content()
    } else {
        Column(
            modifier = modifier
                .fillMaxSize()
                .padding(24.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp, alignment = Alignment.CenterVertically),
        ) {
            Text("Bluetooth permission needed", style = MaterialTheme.typography.headlineSmall)
            Text(
                "MIDI Monitor finds and connects to Bluetooth MIDI devices. Android calls this " +
                    "permission \"Nearby devices\". The app does not use it to work out your location.",
            )
            Button(onClick = { permissionRequest.launch(BLUETOOTH_PERMISSIONS) }) {
                Text("Allow Bluetooth access")
            }
            // After two refusals Android stops showing the dialog, and the only
            // way to grant the permission is through the app's Settings page.
            TextButton(
                onClick = {
                    context.startActivity(
                        Intent(
                            Settings.ACTION_APPLICATION_DETAILS_SETTINGS,
                            Uri.fromParts("package", context.packageName, null),
                        ),
                    )
                },
            ) {
                Text("Open app settings")
            }
        }
    }
}
