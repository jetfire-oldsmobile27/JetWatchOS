package com.jetfire_oldsmobile.jetwatchapp.ui.screens

import android.content.Intent
import android.provider.Settings
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.health.connect.client.PermissionController
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.compose.LifecycleEventEffect
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.jetfire_oldsmobile.jetwatchapp.R
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
fun HomeScreen(
    vm: WatchViewModel,
    onOpenLogs: () -> Unit,
    onOpenStats: () -> Unit,
) {
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
            SectionCard(stringResource(R.string.home_watch_title)) {
                val connText = when (st.conn) {
                    WatchBle.Conn.Ready -> stringResource(R.string.home_connected)
                    WatchBle.Conn.Scanning -> stringResource(R.string.home_scanning)
                    WatchBle.Conn.Connecting -> stringResource(R.string.home_connecting)
                    WatchBle.Conn.Disconnected -> stringResource(R.string.home_disconnected)
                }
                Text(
                    connText +
                            (st.battery?.let { " · " + stringResource(R.string.home_battery, it) } ?: "") +
                            (st.fw?.let { " · " + stringResource(R.string.home_fw, it) } ?: "")
                )
                Text(
                    if (st.lastSync == 0L) stringResource(R.string.home_last_sync_never)
                    else stringResource(
                        R.string.home_last_sync,
                        DateFormat.getTimeInstance(DateFormat.SHORT).format(Date(st.lastSync))
                    ),
                    style = MaterialTheme.typography.bodySmall
                )
                if (st.hcSteps > 0)
                    Text(stringResource(R.string.home_hc_today, st.hcSteps), style = MaterialTheme.typography.bodySmall)
                st.lastError?.let { Text(it, color = MaterialTheme.colorScheme.error) }
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Button(onClick = { vm.sync() }, enabled = !st.syncing && bleOk) {
                        Text(stringResource(R.string.home_sync_now))
                    }
                    if (st.conn == WatchBle.Conn.Ready) OutlinedButton(onClick = { vm.disconnect() }) {
                        Text(stringResource(R.string.home_disconnect))
                    } else OutlinedButton(onClick = { vm.connect(cfg.address == null) }, enabled = bleOk) {
                        Text(if (cfg.address == null) stringResource(R.string.home_find_watch) else stringResource(R.string.home_connect))
                    }
                }
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text(stringResource(R.string.home_bg_service), Modifier.weight(1f))
                    Switch(checked = st.serviceRunning, enabled = bleOk, onCheckedChange = {
                        if (it) WatchService.start(ctx) else WatchService.stop(ctx)
                    })
                }
                TextButton(onClick = onOpenLogs) { Text(stringResource(R.string.home_open_log)) }
            }
        }

        item {
            WeekChart(
                week = st.week.map { it.ymd to it.steps },
                goal = cfg.goal,
                onClick = onOpenStats,
            )
        }

        if (st.trainings.isNotEmpty()) item {
            SectionCard(stringResource(R.string.home_workouts)) {
                st.trainings.take(5).forEach { t ->
                    Text(
                        "${t.type} · ${DateFormat.getDateTimeInstance(DateFormat.SHORT, DateFormat.SHORT).format(Date(t.start * 1000))}" +
                                " · ${t.dur / 60} min · ${t.steps} steps · ${t.dist} m"
                    )
                }
            }
        }

        if (!bleOk || !notifOk || !listenerOk || st.hcStatus != HcStatus.Ready) item {
            SectionCard(stringResource(R.string.home_setup)) {
                if (!bleOk) Button(onClick = { bleLauncher.launch(blePerms()) }) {
                    Text(stringResource(R.string.home_allow_ble))
                }
                if (!notifOk) Button(onClick = { notifLauncher.launch(android.Manifest.permission.POST_NOTIFICATIONS) }) {
                    Text(stringResource(R.string.home_allow_notif))
                }
                if (!listenerOk) Button(onClick = { ctx.startActivity(Intent(Settings.ACTION_NOTIFICATION_LISTENER_SETTINGS)) }) {
                    Text(stringResource(R.string.home_notif_access))
                }
                when (st.hcStatus) {
                    HcStatus.NoPermission -> Button(onClick = { hcLauncher.launch(WatchHub.hc.permissions) }) {
                        Text(stringResource(R.string.home_allow_hc))
                    }
                    HcStatus.NeedsUpdate -> Text(stringResource(R.string.home_hc_update))
                    HcStatus.Unavailable -> Text(stringResource(R.string.home_hc_unavailable))
                    HcStatus.Ready -> {}
                }
            }
        }
        item { Spacer(Modifier.height(16.dp)) }
    }
}

@Composable
private fun WeekChart(week: List<Pair<String, Int>>, goal: Int, onClick: () -> Unit) {
    SectionCard(
        title = stringResource(R.string.home_week_title),
        modifier = Modifier.clickable(onClick = onClick),
    ) {
        if (week.isEmpty()) { Text(stringResource(R.string.home_week_empty)); return@SectionCard }
        val max = maxOf(goal, week.maxOf { it.second }).toFloat()
        Row(
            Modifier.fillMaxWidth().height(130.dp),
            horizontalArrangement = Arrangement.spacedBy(6.dp),
            verticalAlignment = Alignment.Bottom,
        ) {
            week.forEach { (ymd, steps) ->
                Column(
                    Modifier.weight(1f).fillMaxHeight(),
                    horizontalAlignment = Alignment.CenterHorizontally,
                    verticalArrangement = Arrangement.Bottom,
                ) {
                    Text("${steps / 1000}.${(steps % 1000) / 100}k", style = MaterialTheme.typography.labelSmall)
                    Box(
                        Modifier.fillMaxWidth()
                            .fillMaxHeight((steps / max).coerceIn(0.03f, 0.8f))
                            .background(
                                if (steps >= goal) MaterialTheme.colorScheme.tertiary else MaterialTheme.colorScheme.primary,
                                RoundedCornerShape(6.dp)
                            )
                    )
                    Text(
                        runCatching {
                            LocalDate.parse(ymd).dayOfWeek.getDisplayName(TextStyle.SHORT, Locale.getDefault())
                        }.getOrDefault("?"),
                        style = MaterialTheme.typography.labelSmall
                    )
                }
            }
        }
    }
}