package com.jetfire_oldsmobile.jetwatchapp.ui

import androidx.compose.animation.core.*
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import java.text.NumberFormat

/** Circular steps indicator: spinning arc while syncing, goal progress otherwise. */
@Composable
fun StepsRing(steps: Int, goal: Int, syncing: Boolean, modifier: Modifier = Modifier) {
    val progress by animateFloatAsState((steps.toFloat() / goal.coerceAtLeast(1)).coerceIn(0f, 1f), tween(900), label = "progress")
    val spin = rememberInfiniteTransition(label = "spin")
    val angle by spin.animateFloat(0f, 360f, infiniteRepeatable(tween(1100, easing = LinearEasing)), label = "angle")
    val reached = steps >= goal
    val track = MaterialTheme.colorScheme.surfaceVariant
    val color = if (reached) MaterialTheme.colorScheme.tertiary else MaterialTheme.colorScheme.primary

    Box(modifier.size(250.dp), contentAlignment = Alignment.Center) {
        Canvas(Modifier.fillMaxSize()) {
            val stroke = 22.dp.toPx()
            val tl = Offset(stroke / 2, stroke / 2)
            val sz = Size(size.width - stroke, size.height - stroke)
            drawArc(track, 0f, 360f, false, tl, sz, style = Stroke(stroke, cap = StrokeCap.Round))
            if (syncing) drawArc(color, angle - 90f, 110f, false, tl, sz, style = Stroke(stroke, cap = StrokeCap.Round))
            else drawArc(color, -90f, 360f * progress, false, tl, sz, style = Stroke(stroke, cap = StrokeCap.Round))
        }
        Column(horizontalAlignment = Alignment.CenterHorizontally) {
            Text(NumberFormat.getInstance().format(steps), style = MaterialTheme.typography.displayMedium)
            Text(
                if (syncing) "Syncing…" else "of ${NumberFormat.getInstance().format(goal)} steps",
                style = MaterialTheme.typography.bodyMedium, textAlign = TextAlign.Center,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
    }
}

@Composable
fun SectionCard(title: String, modifier: Modifier = Modifier, content: @Composable ColumnScope.() -> Unit) {
    Card(modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
            Text(title, style = MaterialTheme.typography.titleMedium)
            content()
        }
    }
}
