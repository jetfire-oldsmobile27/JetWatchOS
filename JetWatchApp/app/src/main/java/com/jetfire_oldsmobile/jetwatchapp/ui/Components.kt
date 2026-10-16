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
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import com.jetfire_oldsmobile.jetwatchapp.R
import java.text.NumberFormat
import kotlin.math.cos
import kotlin.math.sin

/** Circular steps indicator: spinning arc while syncing, goal progress otherwise. */
@Composable
fun StepsRing(steps: Int, goal: Int, syncing: Boolean, modifier: Modifier = Modifier) {
    val progress by animateFloatAsState(
        (steps.toFloat() / goal.coerceAtLeast(1)).coerceIn(0f, 1f),
        tween(900, easing = FastOutSlowInEasing),
        label = "progress",
    )

    // Два независимых бесконечных трека — как в Material 3 indeterminate:
    //  rotation    — весь круг крутится;
    //  sweepOsc    — длина дуги осциллирует «зигзагом» (как в Android 13+).
    val spin = rememberInfiniteTransition(label = "spin")
    val rotation by spin.animateFloat(
        0f, 360f,
        infiniteRepeatable(tween(2000, easing = LinearEasing)),
        label = "rotation",
    )
    val sweepOsc by spin.animateFloat(
        0f, 1f,
        infiniteRepeatable(
            tween(900, easing = FastOutSlowInEasing),
            RepeatMode.Reverse,
        ),
        label = "sweep",
    )

    val reached = steps >= goal
    val track   = MaterialTheme.colorScheme.surfaceVariant
    val arcCol  = if (reached) MaterialTheme.colorScheme.tertiary
    else         MaterialTheme.colorScheme.primary
    val knob    = Color.White

    Box(modifier.size(250.dp), contentAlignment = Alignment.Center) {
        Canvas(Modifier.fillMaxSize()) {
            val stroke = 22.dp.toPx()
            val tl = Offset(stroke / 2, stroke / 2)
            val sz = Size(size.width - stroke, size.height - stroke)
            val r  = (size.width - stroke) / 2f
            val cx = size.width  / 2f
            val cy = size.height / 2f

            // трек
            drawArc(track, 0f, 360f, false, tl, sz,
                style = Stroke(stroke, cap = StrokeCap.Round))

            if (syncing) {
                // ─── Zig-zag: оборот + осцилляция длины ───
                val sweep = 40f + sweepOsc * 240f          // 40°..280°
                val start = rotation - 90f
                drawArc(arcCol, start, sweep, false, tl, sz,
                    style = Stroke(stroke, cap = StrokeCap.Round))

                // белая «ручка» на переднем крае
                val endRad = Math.toRadians((start + sweep).toDouble())
                val ex = cx + r * cos(endRad).toFloat()
                val ey = cy + r * sin(endRad).toFloat()
                drawCircle(knob,   radius = stroke * 0.38f, center = Offset(ex, ey))
                drawCircle(arcCol, radius = stroke * 0.16f, center = Offset(ex, ey))
            } else {
                val sweep = 360f * progress
                if (sweep > 0.1f) {
                    drawArc(arcCol, -90f, sweep, false, tl, sz,
                        style = Stroke(stroke, cap = StrokeCap.Round))
                }
                // ─── Белая точка-манипулятор на кончике дуги ───
                if (progress > 0.005f) {
                    val endRad = Math.toRadians((-90f + sweep).toDouble())
                    val ex = cx + r * cos(endRad).toFloat()
                    val ey = cy + r * sin(endRad).toFloat()
                    // мягкое «дыхание» точки — визуально приглашает «крутануть»
                    val breathe = 1f + 0.06f * sweepOsc
                    drawCircle(knob,   radius = stroke * 0.44f * breathe, center = Offset(ex, ey))
                    drawCircle(arcCol, radius = stroke * 0.18f, center = Offset(ex, ey))
                }
            }
        }
        Column(horizontalAlignment = Alignment.CenterHorizontally) {
            Text(NumberFormat.getInstance().format(steps),
                style = MaterialTheme.typography.displayMedium)
            Text(
                if (syncing) stringResource(R.string.home_syncing)
                else stringResource(R.string.home_of_goal,
                    NumberFormat.getInstance().format(goal)),
                style = MaterialTheme.typography.bodyMedium,
                textAlign = TextAlign.Center,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
    }
}

@Composable
fun SectionCard(
    title: String,
    modifier: Modifier = Modifier,
    content: @Composable ColumnScope.() -> Unit,
) {
    Card(modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
            Text(title, style = MaterialTheme.typography.titleMedium)
            content()
        }
    }
}