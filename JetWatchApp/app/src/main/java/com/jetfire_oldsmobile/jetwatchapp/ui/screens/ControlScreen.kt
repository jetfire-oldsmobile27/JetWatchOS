package com.jetfire_oldsmobile.jetwatchapp.ui.screens

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.jetfire_oldsmobile.jetwatchapp.R
import com.jetfire_oldsmobile.jetwatchapp.hub.WatchHub
import com.jetfire_oldsmobile.jetwatchapp.proto.AlarmItem
import com.jetfire_oldsmobile.jetwatchapp.ui.CmdButton
import com.jetfire_oldsmobile.jetwatchapp.ui.CmdOutlinedButton
import com.jetfire_oldsmobile.jetwatchapp.ui.InlineSpinner
import com.jetfire_oldsmobile.jetwatchapp.ui.SectionCard
import com.jetfire_oldsmobile.jetwatchapp.ui.WatchViewModel
import kotlinx.coroutines.launch
import java.time.DayOfWeek
import java.time.format.TextStyle
import java.util.Locale

private fun dayShort(i: Int): String =
    DayOfWeek.of(i + 1).getDisplayName(TextStyle.SHORT, Locale.getDefault())

@OptIn(ExperimentalMaterial3Api::class, ExperimentalLayoutApi::class)
@Composable
fun ControlScreen(vm: WatchViewModel) {
    val st by vm.state.collectAsStateWithLifecycle()
    val cfg by vm.settings.collectAsStateWithLifecycle()

    var brightness by remember(cfg.brightness) { mutableFloatStateOf(cfg.brightness.toFloat()) }
    var ssid by remember(cfg.wifiSsid) { mutableStateOf(cfg.wifiSsid) }
    var pwd by remember { mutableStateOf("") }
    var city by remember(cfg.cityName) { mutableStateOf(cfg.cityName) }
    var lat by remember(cfg.lat) { mutableStateOf(cfg.lat.toString()) }
    var lon by remember(cfg.lon) { mutableStateOf(cfg.lon.toString()) }
    var goal by remember(cfg.goal) { mutableFloatStateOf(cfg.goal.toFloat()) }
    var editAlarm by remember { mutableStateOf<AlarmItem?>(null) }
    var confirmBtOff by remember { mutableStateOf(false) }

    val pBright = "brightness"    in st.inFlightCmds
    val pTimeo  = "timeout"       in st.inFlightCmds
    val pWifi   = "wifi"          in st.inFlightCmds
    val pWifiOf = "wifi_off"      in st.inFlightCmds
    val pCity   = "city"          in st.inFlightCmds
    val pAlarm  = "alarm_set"     in st.inFlightCmds
    val pNotify = "notify"        in st.inFlightCmds
    val pPing   = "ping"          in st.inFlightCmds
    val pTimeS  = "time_sync"     in st.inFlightCmds
    val pTrain  = "training_clear" in st.inFlightCmds
    val pSurv   = "survival"      in st.inFlightCmds

    LazyColumn(
        Modifier.fillMaxSize().padding(horizontal = 16.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp),
    ) {
        item { Spacer(Modifier.height(8.dp)) }

        item {
            SectionCard(stringResource(R.string.ctrl_display)) {
                Text(stringResource(R.string.ctrl_brightness, brightness.toInt()))
                Slider(
                    value = brightness,
                    onValueChange = { brightness = it },
                    valueRange = 0f..255f,
                    enabled = !pBright,
                    onValueChangeFinished = {
                        if (!pBright) {
                            vm.cmd("Brightness", "brightness",
                                mapOf("v" to brightness.toInt())) {
                                vm.update { s -> s.copy(brightness = brightness.toInt()) }
                            }
                        }
                    },
                )
                if (pBright) {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        InlineSpinner(Modifier.size(12.dp))
                        Spacer(Modifier.width(6.dp))
                        Text("Sending…", style = MaterialTheme.typography.labelSmall)
                    }
                }
                Text(stringResource(R.string.ctrl_timeout))
                FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    listOf(0 to stringResource(R.string.ctrl_never), 15 to "15 s", 30 to "30 s", 60 to "60 s")
                        .forEach { (s, label) ->
                            FilterChip(
                                selected = cfg.timeoutS == s,
                                enabled  = !pTimeo,
                                onClick  = {
                                    if (!pTimeo) {
                                        vm.cmd("Timeout", "timeout", mapOf("s" to s)) {
                                            vm.update { c -> c.copy(timeoutS = s) }
                                        }
                                    }
                                },
                                label = { Text(label) },
                            )
                        }
                    if (pTimeo) {
                        InlineSpinner(Modifier.size(12.dp))
                    }
                }
            }
        }

        item {
            SectionCard(stringResource(R.string.ctrl_wifi)) {
                OutlinedTextField(ssid, { ssid = it },
                    label = { Text(stringResource(R.string.ctrl_ssid)) },
                    singleLine = true,
                    enabled = !pWifi,
                    modifier = Modifier.fillMaxWidth())
                OutlinedTextField(pwd, { pwd = it },
                    label = { Text(stringResource(R.string.ctrl_password)) },
                    singleLine = true,
                    enabled = !pWifi,
                    visualTransformation = PasswordVisualTransformation(),
                    modifier = Modifier.fillMaxWidth())
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    CmdButton(
                        text    = stringResource(R.string.ctrl_connect),
                        pending = pWifi,
                        enabled = ssid.isNotBlank(),
                    ) {
                        vm.cmd("Wi-Fi", "wifi", mapOf("ssid" to ssid, "pwd" to pwd)) {
                            vm.update { c -> c.copy(wifiSsid = ssid) }
                        }
                    }
                    CmdOutlinedButton(
                        text    = stringResource(R.string.ctrl_wifi_off),
                        pending = pWifiOf,
                    ) {
                        vm.cmd("Wi-Fi off", "wifi_off")
                    }
                }
            }
        }

        item {
            SectionCard(stringResource(R.string.ctrl_city)) {
                OutlinedTextField(city, { city = it },
                    label = { Text(stringResource(R.string.ctrl_name)) },
                    singleLine = true,
                    enabled = !pCity,
                    modifier = Modifier.fillMaxWidth())
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    OutlinedTextField(lat, { lat = it },
                        label = { Text(stringResource(R.string.ctrl_lat)) },
                        singleLine = true,
                        enabled = !pCity,
                        modifier = Modifier.weight(1f),
                        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal))
                    OutlinedTextField(lon, { lon = it },
                        label = { Text(stringResource(R.string.ctrl_lon)) },
                        singleLine = true,
                        enabled = !pCity,
                        modifier = Modifier.weight(1f),
                        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal))
                }
                FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    listOf(
                        Triple("Moscow", 55.75, 37.62),
                        Triple("Saint Petersburg", 59.94, 30.31),
                        Triple("Kazan", 55.79, 49.12),
                    ).forEach { (n, a, o) ->
                        AssistChip(
                            onClick = { city = n; lat = a.toString(); lon = o.toString() },
                            enabled = !pCity,
                            label = { Text(n) },
                        )
                    }
                }
                CmdButton(
                    text    = stringResource(R.string.ctrl_send),
                    pending = pCity,
                ) {
                    val la = lat.toDoubleOrNull(); val lo = lon.toDoubleOrNull()
                    if (la != null && lo != null) {
                        vm.cmd("City", "city",
                            mapOf("name" to city, "lat" to la, "lon" to lo)) {
                            vm.update { c -> c.copy(cityName = city, lat = la, lon = lo) }
                        }
                    }
                }
            }
        }

        item {
            SectionCard(stringResource(R.string.ctrl_alarms)) {
                if (st.alarms.isEmpty()) Text(stringResource(R.string.ctrl_alarm_none))
                st.alarms.sortedBy { it.idx }.forEach { a ->
                    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                        Column(Modifier.weight(1f)) {
                            Text("%02d:%02d".format(a.h, a.m),
                                style = MaterialTheme.typography.titleLarge)
                            Text(
                                (0 until 7).filter { a.hasDay(it) }
                                    .joinToString(" ") { dayShort(it) }
                                    .ifEmpty { stringResource(R.string.ctrl_alarm_once) },
                                style = MaterialTheme.typography.bodySmall,
                            )
                        }
                        TextButton(
                            onClick = { editAlarm = a },
                            enabled = !pAlarm,
                        ) { Text(stringResource(R.string.ctrl_alarm_edit)) }
                        Switch(
                            checked = a.en,
                            enabled = !pAlarm,
                            onCheckedChange = { en -> sendAlarm(vm, a.copy(en = en)) },
                        )
                    }
                }
                if (pAlarm) {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        InlineSpinner(Modifier.size(12.dp))
                        Spacer(Modifier.width(6.dp))
                        Text("Saving…", style = MaterialTheme.typography.labelSmall)
                    }
                }
                CmdOutlinedButton(
                    text    = stringResource(R.string.ctrl_alarm_add),
                    pending = pAlarm,
                ) {
                    editAlarm = AlarmItem(st.alarms.size, 7, 0, true, 0x1F)
                }
            }
        }

        item {
            SectionCard(stringResource(R.string.ctrl_phone_to_watch)) {
                CmdOutlinedButton(
                    text    = stringResource(R.string.ctrl_test_notif),
                    pending = pNotify,
                ) {
                    vm.cmd("Test notification", "notify",
                        mapOf("app" to "JetWatch", "title" to "Test",
                            "body" to "Hello from phone", "ico" to "app"))
                }
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    CmdOutlinedButton(
                        text    = stringResource(R.string.ctrl_ping),
                        pending = pPing,
                    ) { vm.cmd("Ping", "ping") }
                    CmdOutlinedButton(
                        text    = stringResource(R.string.ctrl_time_sync),
                        pending = pTimeS,
                    ) {
                        vm.cmd("Time sync", "time_sync",
                            mapOf("unix" to System.currentTimeMillis() / 1000))
                    }
                }
                CmdOutlinedButton(
                    text    = stringResource(R.string.ctrl_bt_off),
                    pending = false,   // BT-off в диалоге — не блокируем до подтверждения
                ) { confirmBtOff = true }
            }
        }

        item {
            SectionCard(stringResource(R.string.ctrl_power)) {
                Text("${stringResource(R.string.ctrl_cpu)}: —")
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text(stringResource(R.string.ctrl_saver), Modifier.weight(1f))
                    OutlinedButton(onClick = { /* @todo */ }) {
                        Text(stringResource(R.string.ctrl_saver_on))
                    }
                }
                HorizontalDivider(Modifier.padding(vertical = 8.dp))
                Text(
                    "Режим выживания: экран, BLE, WiFi, LoRa и тач отключаются, " +
                            "МК уходит в deep sleep. Метрики — в Serial по боковой кнопке. " +
                            "Выход — двойное нажатие.",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
                var confirmSurv by remember { mutableStateOf(false) }
                CmdOutlinedButton(
                    text    = "Войти в режим выживания",
                    pending = pSurv,
                ) { confirmSurv = true }

                if (confirmSurv) AlertDialog(
                    onDismissRequest = { confirmSurv = false },
                    title = { Text("Режим выживания") },
                    text  = { Text(
                        "Часы перезагрузятся и отключат BLE. Отменить можно " +
                                "только двойным нажатием боковой кнопки. Продолжить?"
                    )},
                    confirmButton = {
                        TextButton(onClick = {
                            confirmSurv = false
                            vm.cmd("Survival", "survival", mapOf("on" to true))
                        }) { Text("Да, взвести") }
                    },
                    dismissButton = {
                        TextButton(onClick = { confirmSurv = false }) { Text("Отмена") }
                    },
                )
            }
        }

        item {
            SectionCard(stringResource(R.string.ctrl_app_settings)) {
                Text(stringResource(R.string.ctrl_goal, goal.toInt()))
                Slider(goal, { goal = it },
                    valueRange = 2000f..30000f, steps = 55,
                    onValueChangeFinished = { vm.update { c -> c.copy(goal = goal.toInt()) } })
                Text(stringResource(R.string.ctrl_sync_interval))
                FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    listOf(5, 15, 30, 60).forEach { m ->
                        FilterChip(cfg.syncIntervalMin == m,
                            { vm.update { c -> c.copy(syncIntervalMin = m) } },
                            label = { Text(stringResource(R.string.ctrl_min, m)) })
                    }
                }
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text(stringResource(R.string.ctrl_clear_after), Modifier.weight(1f))
                    Switch(cfg.clearAfterImport,
                        { v -> vm.update { c -> c.copy(clearAfterImport = v) } })
                }
                CmdOutlinedButton(
                    text    = stringResource(R.string.ctrl_clear_all),
                    pending = pTrain,
                ) {
                    vm.cmd("Clear workouts", "training_clear", mapOf("tid" to 0))
                }
                OutlinedButton(onClick = { vm.forget() }) {
                    Text(stringResource(R.string.ctrl_forget))
                }
            }
        }
        item { Spacer(Modifier.height(16.dp)) }
    }

    editAlarm?.let { a ->
        AlarmDialog(a,
            onDismiss = { editAlarm = null },
            onSave    = { sendAlarm(vm, it); editAlarm = null })
    }
    if (confirmBtOff) AlertDialog(
        onDismissRequest = { confirmBtOff = false },
        title = { Text(stringResource(R.string.ctrl_bt_off_title)) },
        text  = { Text(stringResource(R.string.ctrl_bt_off_text)) },
        confirmButton = {
            TextButton(onClick = {
                confirmBtOff = false
                vm.cmd("Watch BT", "bt", mapOf("on" to false))
            }) { Text(stringResource(R.string.ctrl_bt_off_confirm)) }
        },
        dismissButton = {
            TextButton(onClick = { confirmBtOff = false }) {
                Text(stringResource(R.string.ctrl_cancel))
            }
        },
    )
}

private fun sendAlarm(vm: WatchViewModel, a: AlarmItem) {
    vm.cmd("Alarm", "alarm_set",
        mapOf("idx" to a.idx, "h" to a.h, "m" to a.m,
            "en" to a.en, "days" to a.days)) {
        WatchHub.scope.launch { WatchHub.syncAll() }
    }
}

@OptIn(ExperimentalMaterial3Api::class, ExperimentalLayoutApi::class)
@Composable
private fun AlarmDialog(a: AlarmItem, onDismiss: () -> Unit, onSave: (AlarmItem) -> Unit) {
    val ts = rememberTimePickerState(a.h, a.m, true)
    var days by remember { mutableIntStateOf(a.days) }
    AlertDialog(
        onDismissRequest = onDismiss,
        confirmButton = {
            TextButton(onClick = {
                onSave(a.copy(h = ts.hour, m = ts.minute, en = true, days = days))
            }) { Text(stringResource(R.string.ctrl_save)) }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) { Text(stringResource(R.string.ctrl_cancel)) }
        },
        text = {
            Column(
                Modifier.verticalScroll(rememberScrollState()),
                horizontalAlignment = Alignment.CenterHorizontally,
            ) {
                TimePicker(ts)
                FlowRow(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    (0 until 7).forEach { i ->
                        FilterChip(
                            selected = days and (1 shl i) != 0,
                            onClick  = { days = days xor (1 shl i) },
                            label    = { Text(dayShort(i)) },
                        )
                    }
                }
            }
        },
    )
}