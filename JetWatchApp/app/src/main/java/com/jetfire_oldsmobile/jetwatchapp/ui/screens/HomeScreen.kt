package com.jetfire_oldsmobile.jetwatchapp.ui.screens

import android.content.Intent
import android.provider.Settings
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.background
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.health.connect.client.PermissionController
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.compose.LifecycleEventEffect
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.jetfire_oldsmobile.jetwatchapp.ble.WatchBle
import com.jetfire_oldsmobile.jetwatchapp.health.HcStatus
import com.jetfire_oldsmobile.jetwatchapp.hub.WatchHub
import com.jetfire_oldsmobile.jetwatchapp.service.WatchService
import com.jetfire_oldsmobile.jetwatchapp.ui.*
import java.text.DateFormat
import java.time.LocalDate
import java.time.format.TextStyle
import java.util.Date
import java.util.Locale

@Composable
fun HomeScreen(vm: WatchViewModel, onOpenLogs: () -> Unit) {
    val ctx = LocalContext.current
    val st by vm.state.collectAsStateWithLifecycle()
    val cfg by vm.settings.collectAsStateWithLifecycle()
    var tick by remember { mutableIntStateOf(0) }
    LifecycleEventEffect(Lifecycle.Event.ON_RESUME) { tick++; vm.refreshHealth() }

    val bleLauncher = rememberLauncherForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) { tick++ }
    val notifLauncher = rememberLauncherForActivityResult(ActivityResultContracts.RequestPermission()) { tick++ }
    val hcLauncher = rememberLauncherForActivityResult(PermissionController.createRequestPermissionResultContract()) { vm.refreshHealth() }

    val bleOk = remember(tick) { hasBle(ctx) }
    val notifOk = remember(tick) { hasPostNotif(ctx) }
    val listenerOk = remember(tick) { hasListener(ctx) }

    LazyColumn(
        Modifier.fillMaxSize().padding(horizontal = 16.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        item { Spacer(Modifier.height(8.dp)) }
        item { StepsRing(st.todaySteps, cfg.goal, st.syncing) }

        item {
            SectionCard("Watch") {
                val connText = when (st.conn) {
                    WatchBle.Conn.Ready -> "Connected"
                    WatchBle.Conn.Scanning -> "Searching…"
                    WatchBle.Conn.Connecting -> "Connecting…"
                    WatchBle.Conn.Disconnected -> "Disconnected"
                }
                Text("$connText" + (st.battery?.let { " · battery $it%" } ?: "") + (st.fw?.let { " · fw $it" } ?: ""))
                Text(
                    "Last sync: " + if (st.lastSync == 0L) "never" else DateFormat.getTimeInstance(DateFormat.SHORT).format(Date(st.lastSync)),
                    style = MaterialTheme.typography.bodySmall
                )
                if (st.hcSteps > 0) Text("Health Connect today: ${st.hcSteps} steps (all sources)", style = MaterialTheme.typography.bodySmall)
                st.lastError?.let { Text(it, color = MaterialTheme.colorScheme.error) }
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Button(onClick = { vm.sync() }, enabled = !st.syncing && bleOk) { Text("Sync now") }
                    if (st.conn == WatchBle.Conn.Ready) OutlinedButton(onClick = { vm.disconnect() }) { Text("Disconnect") }
                    else OutlinedButton(onClick = { vm.connect(cfg.address == null) }, enabled = bleOk) {
                        Text(if (cfg.address == null) "Find watch" else "Connect")
                    }
                }
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text("Background service", Modifier.weight(1f))
                    Switch(checked = st.serviceRunning, enabled = bleOk, onCheckedChange = {
                        if (it) WatchService.start(ctx) else WatchService.stop(ctx)
                    })
                }
                TextButton(onClick = onOpenLogs) { Text("Open service.log") }
            }
        }

        item { WeekChart(st.week.map { it.ymd to it.steps }, cfg.goal) }

        if (st.trainings.isNotEmpty()) item {
            SectionCard("Workouts") {
                st.trainings.take(5).forEach { t ->
                    Text(
                        "${t.type} · ${DateFormat.getDateTimeInstance(DateFormat.SHORT, DateFormat.SHORT).format(Date(t.start * 1000))}" +
                            " · ${t.dur / 60} min · ${t.steps} steps · ${t.dist} m"
                    )
                }
            }
        }

        if (!bleOk || !notifOk || !listenerOk || st.hcStatus != HcStatus.Ready) item {
            SectionCard("Setup") {
                if (!bleOk) Button(onClick = { bleLauncher.launch(blePerms()) }) { Text("Allow Bluetooth") }
                if (!notifOk) Button(onClick = { notifLauncher.launch(android.Manifest.permission.POST_NOTIFICATIONS) }) { Text("Allow notifications") }
                if (!listenerOk) Button(onClick = { ctx.startActivity(Intent(Settings.ACTION_NOTIFICATION_LISTENER_SETTINGS)) }) { Text("Notification access (mirroring)") }
                when (st.hcStatus) {
                    HcStatus.NoPermission -> Button(onClick = { hcLauncher.launch(WatchHub.hc.permissions) }) { Text("Allow Health Connect") }
                    HcStatus.NeedsUpdate -> Text("Health Connect needs an update (Play Store).")
                    HcStatus.Unavailable -> Text("Health Connect is not available on this device.")
                    HcStatus.Ready -> {}
                }
            }
        }
        item { Spacer(Modifier.height(16.dp)) }
    }
}

@Composable
private fun WeekChart(week: List<Pair<String, Int>>, goal: Int) {
    SectionCard("Last 7 days") {
        if (week.isEmpty()) { Text("No data yet"); return@SectionCard }
        val max = maxOf(goal, week.maxOf { it.second }).toFloat()
        Row(Modifier.fillMaxWidth().height(130.dp), horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.Bottom) {
            week.forEach { (ymd, steps) ->
                Column(Modifier.weight(1f).fillMaxHeight(), horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.Bottom) {
                    Text("${steps / 1000}.${(steps % 1000) / 100}k", style = MaterialTheme.typography.labelSmall)
                    Box(
                        Modifier.fillMaxWidth().fillMaxHeight((steps / max).coerceIn(0.03f, 0.8f))
                            .background(
                                if (steps >= goal) MaterialTheme.colorScheme.tertiary else MaterialTheme.colorScheme.primary,
                                RoundedCornerShape(6.dp)
                            )
                    )
                    Text(
                        runCatching { LocalDate.parse(ymd).dayOfWeek.getDisplayName(TextStyle.SHORT, Locale.getDefault()) }.getOrDefault("?"),
                        style = MaterialTheme.typography.labelSmall
                    )
                }
            }
        }
    }
}
