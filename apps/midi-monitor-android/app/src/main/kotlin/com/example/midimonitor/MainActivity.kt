package com.example.midimonitor

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.dynamicDarkColorScheme
import androidx.compose.material3.dynamicLightColorScheme
import androidx.activity.compose.BackHandler
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.lifecycle.viewmodel.compose.viewModel
import com.example.midimonitor.ui.BluetoothPermissionGate
import com.example.midimonitor.ui.MonitorScreen
import com.example.midimonitor.ui.ScanScreen

/**
 * An Activity is one screen that Android can start. This app has exactly one;
 * everything the user sees is drawn inside it by Compose.
 */
class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Draw behind the status and navigation bars; Scaffold below hands back
        // the padding needed to keep content clear of them.
        enableEdgeToEdge()

        // setContent replaces the classic XML layout. The lambda is the root of
        // the Compose UI tree.
        setContent {
            MidiMonitorApp()
        }
    }
}

/**
 * The root of the UI. A @Composable function describes a piece of UI as a
 * function of its inputs; Compose calls it again ("recomposes") whenever state
 * it reads has changed.
 */
@Composable
fun MidiMonitorApp() {
    // Follow the phone's light/dark setting and take colours from its wallpaper
    // ("dynamic colour", available on every Android version this app supports).
    val context = LocalContext.current
    val colors = if (isSystemInDarkTheme()) dynamicDarkColorScheme(context) else dynamicLightColorScheme(context)

    // Which screen is showing. rememberSaveable is `remember` that also survives
    // the Activity being recreated, for example on rotation.
    var screen by rememberSaveable { mutableStateOf(Screen.Scan) }

    // Created here, above both screens, so the monitor keeps its log while the
    // scan screen is showing.
    val monitorViewModel: MonitorViewModel = viewModel()

    MaterialTheme(colorScheme = colors) {
        Scaffold { innerPadding ->
            BluetoothPermissionGate(Modifier.padding(innerPadding)) {
                when (screen) {
                    Screen.Scan -> ScanScreen(
                        onDeviceClick = { device ->
                            monitorViewModel.connect(device)
                            screen = Screen.Monitor
                        },
                        onDemoClick = {
                            monitorViewModel.startDemo()
                            screen = Screen.Monitor
                        },
                        modifier = Modifier.padding(innerPadding),
                    )

                    Screen.Monitor -> {
                        val leave = {
                            monitorViewModel.stop()
                            screen = Screen.Scan
                        }
                        // Without this the system back gesture would close the app.
                        BackHandler(onBack = leave)
                        MonitorScreen(monitorViewModel, onBack = leave, modifier = Modifier.padding(innerPadding))
                    }
                }
            }
        }
    }
}

/**
 * With only two screens, a variable and a `when` is all the navigation needed.
 * The Navigation library earns its keep once there are deep links or a back stack.
 */
private enum class Screen { Scan, Monitor }
