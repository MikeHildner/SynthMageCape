package com.example.midimonitor.ui

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.example.midimonitor.midi.HeldNotes
import com.example.midimonitor.midi.noteName

/**
 * Shows which keys are down, how many, and what the sustain pedal is doing.
 *
 * The panel always has the same three lines, even when they are empty, so that
 * the log underneath does not jump up and down as notes come and go.
 */
@Composable
fun HeldNotesPanel(heldNotes: HeldNotes, modifier: Modifier = Modifier) {
    // Surface is a coloured, shaped background. surfaceContainerHigh is a theme
    // colour a step away from the page background, which sets the panel apart.
    Surface(
        modifier = modifier.fillMaxWidth(),
        color = MaterialTheme.colorScheme.surfaceContainerHigh,
        shape = MaterialTheme.shapes.medium,
    ) {
        Column(Modifier.padding(12.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                val count = heldNotes.down.size
                Text(
                    text = if (count == 1) "1 key down" else "$count keys down",
                    style = MaterialTheme.typography.titleMedium,
                )
                Spacer(Modifier.weight(1f))
                Text(
                    text = if (heldNotes.pedalDown) "Sustain pedal down" else "Sustain pedal up",
                    style = MaterialTheme.typography.labelLarge,
                    color = if (heldNotes.pedalDown) {
                        MaterialTheme.colorScheme.primary
                    } else {
                        MaterialTheme.colorScheme.onSurfaceVariant
                    },
                )
            }

            Text(
                text = heldNotes.downNotes.joinToString("  ") { noteName(it) }.ifEmpty { "–" },
                style = MaterialTheme.typography.headlineSmall,
                maxLines = 1,
            )

            Text(
                text = "Held by pedal: " + heldNotes.sustainedNotes.joinToString("  ") { noteName(it) }.ifEmpty { "–" },
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                maxLines = 1,
            )
        }
    }
}
