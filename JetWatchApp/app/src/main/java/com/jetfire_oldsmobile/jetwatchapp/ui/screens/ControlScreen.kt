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
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.jetfire_oldsmobile.jetwatchapp.hub.WatchHub
import com.jetfire_oldsmobile.jetwatchapp.proto.AlarmItem
import kotlinx.coroutines.launch
import com.jetfire_oldsmobile.jetwatchapp.ui.SectionCard
import com.jetfire_oldsmobile.jetwatchapp.ui.WatchViewModel

private val DAY_NAMES = listOf("Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun")

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

    LazyColumn(Modifier.fillMaxSize().padding(horizontal = 16.dp), verticalArrangement = Arrangement.spacedBy(14.dp)) {
        item { Spacer(Modifier.height(8.dp)) }

        item {
            SectionCard("Display") {
                Text("Brightness ${brightness.toInt()}")
                Slider(brightness, { brightness = it }, valueRange = 0f..255f, onValueChangeFinished = {
                    vm.cmd("Brightness", "brightness", mapOf("v" to brightness.toInt())) { vm.update { s -> s.copy(brightness = brightness.toInt()) } }
                })
                Text("Screen timeout")
                FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    listOf(0 to "Never", 15 to "15 s", 30 to "30 s", 60 to "60 s").forEach { (s, label) ->
                        FilterChip(selected = cfg.timeoutS == s, onClick = {
                            vm.cmd("Timeout", "timeout", mapOf("s" to s)) { vm.update { c -> c.copy(timeoutS = s) } }
                        }, label = { Text(label) })
                    }
                }
            }
        }

        item {
            SectionCard("Wi-Fi") {
                OutlinedTextField(ssid, { ssid = it }, label = { Text("SSID") }, singleLine = true, modifier = Modifier.fillMaxWidth())
                OutlinedTextField(pwd, { pwd = it }, label = { Text("Password") }, singleLine = true,
                    visualTransformation = PasswordVisualTransformation(), modifier = Modifier.fillMaxWidth())
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Button(enabled = ssid.isNotBlank(), onClick = {
                        vm.cmd("Wi-Fi", "wifi", mapOf("ssid" to ssid, "pwd" to pwd)) { vm.update { c -> c.copy(wifiSsid = ssid) } }
                    }) { Text("Connect") }
                    OutlinedButton(onClick = { vm.cmd("Wi-Fi off", "wifi_off") }) { Text("Wi-Fi off") }
                }
            }
        }

        item {
            SectionCard("City / weather") {
                OutlinedTextField(city, { city = it }, label = { Text("Name") }, singleLine = true, modifier = Modifier.fillMaxWidth())
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    OutlinedTextField(lat, { lat = it }, label = { Text("Lat") }, singleLine = true, modifier = Modifier.weight(1f),
                        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal))
                    OutlinedTextField(lon, { lon = it }, label = { Text("Lon") }, singleLine = true, modifier = Modifier.weight(1f),
                        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal))
                }
                FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    listOf(Triple("Moscow", 55.75, 37.62), Triple("Saint Petersburg", 59.94, 30.31), Triple("Kazan", 55.79, 49.12))
                        .forEach { (n, a, o) -> AssistChip(onClick = { city = n; lat = a.toString(); lon = o.toString() }, label = { Text(n) }) }
                }
                Button(onClick = {
                    val la = lat.toDoubleOrNull(); val lo = lon.toDoubleOrNull()
                    if (la != null && lo != null) vm.cmd("City", "city", mapOf("name" to city, "lat" to la, "lon" to lo)) {
                        vm.update { c -> c.copy(cityName = city, lat = la, lon = lo) }
                    }
                }) { Text("Send to watch") }
            }
        }

        item {
            SectionCard("Alarms") {
                if (st.alarms.isEmpty()) Text("No alarms loaded (connect and sync).")
                st.alarms.sortedBy { it.idx }.forEach { a ->
                    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                        Column(Modifier.weight(1f)) {
                            Text("%02d:%02d".format(a.h, a.m), style = MaterialTheme.typography.titleLarge)
                            Text(DAY_NAMES.filterIndexed { i, _ -> a.hasDay(i) }.joinToString(" ").ifEmpty { "Once" },
                                style = MaterialTheme.typography.bodySmall)
                        }
                        TextButton(onClick = { editAlarm = a }) { Text("Edit") }
                        Switch(a.en, { en -> sendAlarm(vm, a.copy(en = en)) })
                    }
                }
                OutlinedButton(onClick = { editAlarm = AlarmItem(st.alarms.size, 7, 0, true, 0x1F) }) { Text("Add alarm") }
            }
        }

        item {
            SectionCard("Phone → watch") {
                OutlinedButton(onClick = {
                    vm.cmd("Test notification", "notify", mapOf("app" to "JetWatch", "title" to "Test", "body" to "Hello from phone", "ico" to "app"))
                }) { Text("Send test notification") }
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    OutlinedButton(onClick = { vm.cmd("Ping", "ping") }) { Text("Ping") }
                    OutlinedButton(onClick = { vm.cmd("Time sync", "time_sync", mapOf("unix" to System.currentTimeMillis() / 1000)) }) { Text("Sync time") }
                }
                OutlinedButton(onClick = { confirmBtOff = true }) { Text("Turn watch Bluetooth off…") }
            }
        }

        item {
            SectionCard("App settings") {
                Text("Daily goal: ${goal.toInt()} steps")
                Slider(goal, { goal = it }, valueRange = 2000f..30000f, steps = 55,
                    onValueChangeFinished = { vm.update { c -> c.copy(goal = goal.toInt()) } })
                Text("Sync interval")
                FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    listOf(5, 15, 30, 60).forEach { m ->
                        FilterChip(cfg.syncIntervalMin == m, { vm.update { c -> c.copy(syncIntervalMin = m) } }, label = { Text("$m min") })
                    }
                }
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text("Erase workouts on watch after import", Modifier.weight(1f))
                    Switch(cfg.clearAfterImport, { v -> vm.update { c -> c.copy(clearAfterImport = v) } })
                }
                OutlinedButton(onClick = { vm.cmd("Clear workouts", "training_clear", mapOf("tid" to 0)) }) { Text("Erase all workouts on watch") }
                OutlinedButton(onClick = { vm.forget() }) { Text("Forget this watch") }
            }
        }
        item { Spacer(Modifier.height(16.dp)) }
    }

    editAlarm?.let { a ->
        AlarmDialog(a, onDismiss = { editAlarm = null }, onSave = { sendAlarm(vm, it); editAlarm = null })
    }
    if (confirmBtOff) AlertDialog(
        onDismissRequest = { confirmBtOff = false },
        title = { Text("Turn Bluetooth off?") },
        text = { Text("The watch will drop this connection and the phone won't be able to reconnect until you enable BLE on the watch.") },
        confirmButton = { TextButton(onClick = { confirmBtOff = false; vm.cmd("Watch BT", "bt", mapOf("on" to false)) }) { Text("Turn off") } },
        dismissButton = { TextButton(onClick = { confirmBtOff = false }) { Text("Cancel") } },
    )
}

private fun sendAlarm(vm: WatchViewModel, a: AlarmItem) {
    vm.cmd("Alarm", "alarm_set", mapOf("idx" to a.idx, "h" to a.h, "m" to a.m, "en" to a.en, "days" to a.days)) {
        WatchHub.scope.launch { WatchHub.syncAll() } // refresh alarm list
    }
}

@OptIn(ExperimentalMaterial3Api::class, ExperimentalLayoutApi::class)
@Composable
private fun AlarmDialog(a: AlarmItem, onDismiss: () -> Unit, onSave: (AlarmItem) -> Unit) {
    val ts = rememberTimePickerState(a.h, a.m, true)
    var days by remember { mutableIntStateOf(a.days) }
    AlertDialog(
        onDismissRequest = onDismiss,
        confirmButton = { TextButton(onClick = { onSave(a.copy(h = ts.hour, m = ts.minute, en = true, days = days)) }) { Text("Save") } },
        dismissButton = { TextButton(onClick = onDismiss) { Text("Cancel") } },
        text = {
            Column(Modifier.verticalScroll(rememberScrollState()), horizontalAlignment = Alignment.CenterHorizontally) {
                TimePicker(ts)
                FlowRow(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    DAY_NAMES.forEachIndexed { i, n ->
                        FilterChip(days and (1 shl i) != 0, { days = days xor (1 shl i) }, label = { Text(n) })
                    }
                }
            }
        },
    )
}
