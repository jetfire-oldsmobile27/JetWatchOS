package com.jetfire_oldsmobile.jetwatchapp.hub

import android.annotation.SuppressLint
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
    /** Локальная дата, к которой относятся накопленные счётчики. При смене дня сбрасываем. */
    val stepsDate: String = LocalDate.now().toString(),
    val powerSave: Boolean = false,
    val inFlightCmds: Set<String> = emptySet(),
) {
    /**
     * Приоритет: телефон (Health Connect, все источники) → данные часов за сегодня → максимум из недели.
     * Никогда не берём меньше уже накопленного за сегодня.
     */
    val todaySteps: Int
        get() {
            val today = LocalDate.now().toString()
            val stale = stepsDate != today
            val fromWatch = if (stale) 0 else (watchSteps ?: 0)
            val fromWeek = week.firstOrNull { it.ymd == today }?.steps ?: 0
            val fromHc = hcSteps.toInt()
            return maxOf(fromWatch, fromWeek, fromHc)
        }
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
                // Смена суток: сбрасываем счётчики, чтобы не тащить вчерашнее.
                val today = LocalDate.now().toString()
                if (_state.value.stepsDate != today) {
                    AppLog.i("HUB", "new day $today -> reset watch counters")
                    _state.update { it.copy(stepsDate = today, watchSteps = null, hcSteps = 0) }
                    if (ble.conn.value == WatchBle.Conn.Ready) syncAll() 
                }

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

    @SuppressLint("StaticFieldLeak")
    suspend fun syncAll() = syncMutex.withLock {
        if (ble.conn.value != WatchBle.Conn.Ready) return@withLock
        val t0 = System.currentTimeMillis()
        _state.update { it.copy(syncing = true, lastError = null) }
        try {
            hcWritable = runCatching { hc.status() == HcStatus.Ready }.getOrDefault(false)

            ble.request("ping")?.takeIf { it.optBoolean("ok") }?.let { p ->
                _state.update { 
                    it.copy(
                        fw = p.optString("fw"), 
                        battery = p.optInt("batt", -1).takeIf { b -> b >= 0 },
                        powerSave = p.optBoolean("ps", it.powerSave)
                        ) 
                }
            }
            val zone = ZoneId.systemDefault()
            ble.request("time_sync", mapOf(
                "unix" to System.currentTimeMillis() / 1000,
                "tz" to zone.rules.getOffset(java.time.Instant.now()).totalSeconds,
            ))

            // 30 дней истории (в карточке показываем 7, в отдельном экране — все).
            ble.request("steps", mapOf("days" to 7))?.takeIf { it.optBoolean("ok") }?.let { r ->
                val days = r.daysList().sortedBy { it.ymd }
                val today = LocalDate.now().toString()
                val watchToday = days.firstOrNull { it.ymd == today }?.steps ?: 0

                // Свежее значение из Health Connect (все источники, дедупликация уже сделана HC).
                // Если фоновое чтение недоступно — берём то, что уже в стейте.
                val hcTodayFresh = if (hcWritable)
                    runCatching { hc.readTodaySteps() }.getOrDefault(-1L)
                else -1L
                val hcToday = if (hcTodayFresh >= 0) hcTodayFresh else _state.value.hcSteps

                // Телефон — источник истины; часы только добавляют, если телефон молчит.
                val totalToday = maxOf(hcToday, watchToday.toLong())
                if (hcToday > watchToday) {
                    AppLog.i("HUB", "push steps to watch: watch=$watchToday phone=$hcToday")
                    runCatching {
                        ble.request("steps_set", mapOf("steps" to hcToday))?.let { r ->
                            if (r.optBoolean("ok")) {
                                _state.update { st -> st.copy(watchSteps = hcToday.toInt()) }
                            }
                        }
                    }.onFailure { AppLog.w("HUB", "steps_set failed: ${it.message}") }
                }

                _state.update { st ->
                    val newDay = st.stepsDate != today
                    val prevWatch = if (newDay) 0 else (st.watchSteps ?: 0)
                    val prevHc = if (newDay) 0L else st.hcSteps

                    // Если часовой `days` пуст или не содержит сегодня — вставим сегодняшний
                    // вручную, чтобы WeekChart не показывал «нет данных».
                    val week = if (days.isEmpty() || days.none { it.ymd == today }) {
                        (days + DayStepsItem(today, maxOf(watchToday, totalToday.toInt())))
                            .sortedBy { it.ymd }
                            .takeLast(7)
                    } else days

                    st.copy(
                        week = week,
                        stepsDate = today,
                        watchSteps = if (newDay) watchToday else maxOf(prevWatch, watchToday),
                        hcSteps = if (newDay) totalToday else maxOf(prevHc, totalToday),
                    )
                }

                if (hcWritable) {
                    // Сегодня пишем МАКСИМУМ, чтобы 0 с зависших часов не обнулил данные телефона.
                    if (totalToday > 0) {
                        runCatching { hc.writeDailySteps(today, totalToday) }
                            .onFailure { AppLog.e("HC", "write steps $today", it) }
                    }
                    // Прошлые дни — только если там реально что-то есть.
                    days.filter { it.ymd != today && it.steps > 0 }.forEach { d ->
                        runCatching { hc.writeDailySteps(d.ymd, d.steps.toLong()) }
                            .onFailure { AppLog.e("HC", "write steps ${d.ymd}", it) }
                    }
                } else AppLog.w("HC", "no Health Connect write permission, skipping steps write")
            }

            importTrainings()

            ble.request("alarm_get", timeoutMs = 12_000)?.takeIf { it.optBoolean("ok") }?.let { r ->
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
            val left = 900 - (System.currentTimeMillis() - t0)
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
            if (s.clearAfterImport) ble.request("training_clear", mapOf("tid" to 0))
        }
    }

    /** Refresh Health Connect status + today's total (UI only; background reads need extra permission). */
    suspend fun refreshHealth() {
        val st = runCatching { hc.status() }.getOrDefault(HcStatus.Unavailable)
        val steps = if (st == HcStatus.Ready) runCatching { hc.readTodaySteps() }.getOrDefault(0L) else 0L
        val today = LocalDate.now().toString()
        _state.update { s ->
            val newDay = s.stepsDate != today
            s.copy(
                hcStatus = st,
                hcSteps = if (newDay) steps else maxOf(s.hcSteps, steps),
                stepsDate = today,
            )
        }
    }

    // ---------- commands from UI ----------

    suspend fun command(name: String, extras: Map<String, Any?> = emptyMap()): JSONObject? {
        var reserved = false
        _state.update { st ->
            if (name in st.inFlightCmds) st
            else { reserved = true; st.copy(inFlightCmds = st.inFlightCmds + name) }
        }
        if (!reserved) {
            AppLog.w("HUB", "cmd '$name' already in flight, ignored")
            return null
        }
        try {
            if (ble.conn.value != WatchBle.Conn.Ready && !connectNow()) return null
            return ble.request(name, extras)
        } finally {
            _state.update { it.copy(inFlightCmds = it.inFlightCmds - name) }
        }
    }

    // ---------- events from the watch ----------

    private fun onEvent(ev: JSONObject) {
        when (ev.optString("ev")) {
            "step" -> {
                val today = LocalDate.now().toString()
                val total = ev.optInt("total")
                _state.update { st ->
                    val newDay = st.stepsDate != today
                    val prev = if (newDay) 0 else (st.watchSteps ?: 0)
                    val w = if (newDay) total else maxOf(prev, total)
                    val h = if (newDay) w.toLong() else maxOf(st.hcSteps, w.toLong())
                    st.copy(stepsDate = today, watchSteps = w, hcSteps = h)
                }
                if (hcWritable) scope.launch {
                    val toWrite = _state.value.hcSteps
                    if (toWrite > 0) runCatching { hc.writeDailySteps(today, toWrite) }
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
                "pkg" to pkg, "ico" to Proto.iconKey(pkg),
            ))
        }
    }
}