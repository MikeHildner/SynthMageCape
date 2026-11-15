package com.example.midimonitor.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.example.midimonitor.MonitorUiState
import com.example.midimonitor.MonitorViewModel
import com.example.midimonitor.PacketEntry

@Composable
fun MonitorScreen(viewModel: MonitorViewModel, onBack: () -> Unit, modifier: Modifier = Modifier) {
    val state by viewModel.state.collectAsStateWithLifecycle()
    MonitorContent(
        state = state,
        onBack = onBack,
        onPausedChange = viewModel::setPaused,
        onShowHexChange = viewModel::setShowHex,
        onClear = viewModel::clear,
        modifier = modifier,
    )
}

@Composable
private fun MonitorContent(
    state: MonitorUiState,
    onBack: () -> Unit,
    onPausedChange: (Boolean) -> Unit,
    onShowHexChange: (Boolean) -> Unit,
    onClear: () -> Unit,
    modifier: Modifier = Modifier,
) {
    Column(
        modifier = modifier
            .fillMaxSize()
            .padding(horizontal = 16.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text(state.source, style = MaterialTheme.typography.headlineSmall)
            Spacer(Modifier.weight(1f))
            TextButton(onClick = onBack) { Text("Back") }
        }

        Text(
            text = state.status,
            style = MaterialTheme.typography.labelLarge,
            color = if (state.statusIsError) MaterialTheme.colorScheme.error else MaterialTheme.colorScheme.primary,
        )

        HeldNotesPanel(state.heldNotes)

        Text(
            text = buildString {
                append("${state.packetCount} packets  ·  ${state.messageCount} messages  ·  ${state.clockTicks} clock ticks")
                if (state.problemCount == 1) append("  ·  1 problem")
                if (state.problemCount > 1) append("  ·  ${state.problemCount} problems")
            },
            style = MaterialTheme.typography.labelLarge,
        )

        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            OutlinedButton(onClick = { onPausedChange(!state.paused) }) {
                Text(if (state.paused) "Resume" else "Pause")
            }
            OutlinedButton(onClick = onClear) { Text("Clear") }
            Spacer(Modifier.weight(1f))
            Text("Hex")
            Switch(checked = state.showHex, onCheckedChange = onShowHexChange)
        }

        HorizontalDivider()

        // Newest packet first. No `key` here, on purpose: without keys a
        // LazyColumn holds its scroll position by index, so while it is scrolled
        // to the top each new packet simply appears there. With keys it would
        // stay anchored to the row you were looking at and new rows would pile
        // up out of sight above it.
        LazyColumn(Modifier.fillMaxSize()) {
            items(state.entries) { entry ->
                PacketRow(entry, state.showHex)
                HorizontalDivider()
            }
        }
    }
}

@Composable
private fun PacketRow(entry: PacketEntry, showHex: Boolean) {
    Column(
        Modifier
            .fillMaxWidth()
            .padding(vertical = 6.dp),
    ) {
        if (showHex) {
            Text(
                text = entry.hex,
                fontFamily = FontFamily.Monospace,
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }

        for (line in entry.lines) {
            Row {
                Text(
                    text = line.channel?.let { "ch $it" } ?: "sys",
                    modifier = Modifier.width(48.dp),
                    color = MaterialTheme.colorScheme.primary,
                )
                Text(line.text, modifier = Modifier.weight(1f))
                Text(
                    text = "${line.timestamp} ms",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }

        for (problem in entry.problems) {
            Text("Problem: $problem", color = MaterialTheme.colorScheme.error)
        }
    }
}
