package com.jetfire_oldsmobile.jetwatchapp.hub

import android.content.Context
import com.jetfire_oldsmobile.jetwatchapp.ble.WatchBle
import com.jetfire_oldsmobile.jetwatchapp.data.Settings
import com.jetfire_oldsmobile.jetwatchapp.data.SettingsStore
import com.jetfire_oldsmobile.jetwatchapp.health.HcStatus
import com.jetfire_oldsmobile.jetwatchapp.health.HealthConnectManager
import com.jetfire_oldsmobile.jetwatchapp.log.AppLog
import com.jetfire_oldsmobile.jetwatchapp.proto.*
import com.jetfire_oldsmobile.jetwatchapp.service.Notif
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.*
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import org.json.JSONObject
import java.time.LocalDate
import java.time.ZoneId

data class WatchState(
    val conn: WatchBle.Conn = WatchBle.Conn.Disconnected,
    val serviceRunning: Boolean = false,
    val syncing: Boolean = false,
    val watchSteps: Int? = null,
    val hcSteps: Long = 0,
    val battery: Int? = null,
    val fw: String? = null,
    val lastSync: Long = 0,
    val week: List<DayStepsItem> = emptyList(),
    val trainings: List<Training> = emptyList(),
    val alarms: List<AlarmItem> = emptyList(),
    val hcStatus: HcStatus = HcStatus.NoPermission,
    val lastError: String? = null,
) {
    val todaySteps: Int
        get() = watchSteps ?: week.lastOrNull()?.steps ?: hcSteps.toInt()
}

/** Process-wide singleton shared by UI, foreground service and notification listener. */
object WatchHub {
    private lateinit var app: Context
    val scope = CoroutineScope(SupervisorJob() + Dispatchers.Default)
    lateinit var ble: WatchBle
    lateinit var hc: HealthConnectManager
    private lateinit var store: SettingsStore

    private val _settings = MutableStateFlow(Settings())
    val settings = _settings.asStateFlow()
    private val _state = MutableStateFlow(WatchState())
    val state = _state.asStateFlow()

    private var loopJob: Job? = null
    private val syncMutex = Mutex()
    private var hcWritable = false

    fun init(ctx: Context) {
        app = ctx.applicationContext
        store = SettingsStore(app)
        _settings.value = store.load()
        ble = WatchBle(app)
        hc = HealthConnectManager(app)
        ble.conn.onEach { c ->
            AppLog.i("HUB", "connection -> $c")
            _state.update { it.copy(conn = c) }
        }.launchIn(scope)
        ble.events.onEach(::onEvent).launchIn(scope)
    }

    fun updateSettings(block: (Settings) -> Settings) {
        _settings.update(block)
        store.save(_settings.value)
    }

    // ---------- background loop (owned by WatchService) ----------

    fun startLoop() {
        if (loopJob?.isActive == true) return
        _state.update { it.copy(serviceRunning = true) }
        loopJob = scope.launch { runLoop() }
    }

    fun stopLoop() {
        loopJob?.cancel(); loopJob = null
        ble.disconnect()
        _state.update { it.copy(serviceRunning = false) }
    }

    private suspend fun runLoop() {
        AppLog.i("HUB", "loop started")
        while (currentCoroutineContext().isActive) {
            try {
                if (ble.conn.value != WatchBle.Conn.Ready) connectNow()
                val s = settings.value
                val due = System.currentTimeMillis() - state.value.lastSync > s.syncIntervalMin * 60_000L
                if (ble.conn.value == WatchBle.Conn.Ready && due) syncAll()
            } catch (e: CancellationException) {
                throw e
            } catch (e: Exception) {
                AppLog.e("HUB", "loop error", e)
            }
            delay(if (ble.conn.value == WatchBle.Conn.Ready) 30_000 else 45_000)
        }
    }

    suspend fun connectNow(scanNew: Boolean = false): Boolean {
        if (scanNew) updateSettings { it.copy(address = null) }
        val ok = ble.connect(settings.value.address)
        if (ok) {
            ble.deviceAddress?.let { a -> updateSettings { it.copy(address = a) } }
            syncAll()
        }
        return ok
    }

    fun forget() {
        ble.disconnect()
        updateSettings { it.copy(address = null) }
    }

    // ---------- sync ----------

    suspend fun syncAll() = syncMutex.withLock {
        if (ble.conn.value != WatchBle.Conn.Ready) return@withLock
        val t0 = System.currentTimeMillis()
        _state.update { it.copy(syncing = true, lastError = null) }
        try {
            hcWritable = runCatching { hc.status() == HcStatus.Ready }.getOrDefault(false)

            ble.request("ping")?.takeIf { it.optBoolean("ok") }?.let { p ->
                _state.update { it.copy(fw = p.optString("fw"), battery = p.optInt("batt", -1).takeIf { b -> b >= 0 }) }
            }
            val zone = ZoneId.systemDefault()
            ble.request("time_sync", mapOf(
                "unix" to System.currentTimeMillis() / 1000,
                "tz" to zone.rules.getOffset(java.time.Instant.now()).totalSeconds, // extra field, see notes
            ))

            ble.request("steps", mapOf("days" to 7))?.takeIf { it.optBoolean("ok") }?.let { r ->
                val days = r.daysList().sortedBy { it.ymd }
                val today = LocalDate.now().toString()
                _state.update { st ->
                    st.copy(week = days, watchSteps = days.firstOrNull { it.ymd == today }?.steps ?: st.watchSteps)
                }
                if (hcWritable) days.forEach { d ->
                    runCatching { hc.writeDailySteps(d.ymd, d.steps.toLong()) }
                        .onFailure { AppLog.e("HC", "write steps ${d.ymd}", it) }
                } else AppLog.w("HC", "no Health Connect write permission, skipping steps write")
            }

            importTrainings()

            ble.request("alarm_get")?.takeIf { it.optBoolean("ok") }?.let { r ->
                _state.update { it.copy(alarms = r.alarmsList()) }
            }
            _state.update { it.copy(lastSync = System.currentTimeMillis()) }
            AppLog.i("HUB", "sync done")
        } catch (e: CancellationException) {
            throw e
        } catch (e: Exception) {
            AppLog.e("HUB", "sync failed", e)
            _state.update { it.copy(lastError = e.message) }
        } finally {
            val left = 900 - (System.currentTimeMillis() - t0) // keep ring animation visible
            if (left > 0) delay(left)
            _state.update { it.copy(syncing = false) }
        }
    }

    private suspend fun importTrainings() {
        val r = ble.request("training_list")?.takeIf { it.optBoolean("ok") } ?: return
        val all = r.trainingList().sortedByDescending { it.start }
        _state.update { it.copy(trainings = all.take(20)) }
        val s = settings.value
        val fresh = all.filter { it.start > s.lastTrainingStart }
        if (fresh.isEmpty() || !hcWritable) return
        var ok = true
        fresh.forEach { t -> runCatching { hc.writeTraining(t) }.onFailure { ok = false; AppLog.e("HC", "write training ${t.id}", it) } }
        if (ok) {
            updateSettings { it.copy(lastTrainingStart = fresh.maxOf { t -> t.start }) }
            if (s.clearAfterImport) ble.request("training_clear", mapOf("tid" to 0)) // see notes: "id" clashes with request id
        }
    }

    /** Refresh Health Connect status + today's total (UI only; background reads need extra permission). */
    suspend fun refreshHealth() {
        val st = runCatching { hc.status() }.getOrDefault(HcStatus.Unavailable)
        val steps = if (st == HcStatus.Ready) runCatching { hc.readTodaySteps() }.getOrDefault(0L) else 0L
        _state.update { it.copy(hcStatus = st, hcSteps = steps) }
    }

    // ---------- commands from UI ----------

    suspend fun command(name: String, extras: Map<String, Any?> = emptyMap()): JSONObject? {
        if (ble.conn.value != WatchBle.Conn.Ready && !connectNow()) return null
        return ble.request(name, extras)
    }

    // ---------- events from the watch ----------

    private fun onEvent(ev: JSONObject) {
        when (ev.optString("ev")) {
            "step" -> {
                val total = ev.optInt("total")
                _state.update { it.copy(watchSteps = total) }
                if (hcWritable) scope.launch {
                    runCatching { hc.writeDailySteps(LocalDate.now().toString(), total.toLong()) }
                }
            }
            "batt" -> {
                val pct = ev.optInt("pct"); val low = ev.optBoolean("low")
                _state.update { it.copy(battery = pct) }
                if (low) Notif.alert(app, 2, "Watch battery low", "T-Watch is at $pct%")
            }
            "training_start" -> AppLog.i("HUB", "training started: ${ev.optString("type")}")
            "training_end" -> scope.launch { syncMutex.withLock { importTrainings() } }
            "alarm" -> AppLog.i("HUB", "alarm fired idx=${ev.optInt("idx")}")
        }
    }

    // ---------- notification mirroring ----------

    private var lastKey = ""
    private var lastKeyAt = 0L

    fun mirror(pkg: String, appName: String, title: String, body: String) {
        val s = settings.value
        if (!s.mirrorEnabled || pkg !in s.allowedPkgs) return
        val key = "$pkg|$title|$body"
        val now = System.currentTimeMillis()
        if (key == lastKey && now - lastKeyAt < 3000) return
        lastKey = key; lastKeyAt = now
        scope.launch {
            if (ble.conn.value != WatchBle.Conn.Ready) {
                AppLog.w("MIRROR", "watch not connected, dropped notification from $pkg"); return@launch
            }
            ble.request("notify", mapOf(
                "app" to appName, "title" to title.take(40), "body" to body.take(160),
                "pkg" to pkg, "ico" to Proto.iconKey(pkg), // extra fields, see notes
            ))
        }
    }
}
