package com.jetfire_oldsmobile.jetwatchapp.log

import android.content.Context
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * File logger: <externalFilesDir>/logs/service.log  (Android/data/<package>/files/logs/service.log).
 * Never touches logcat. The file is truncated automatically when the calendar day changes.
 */
object AppLog {
    lateinit var file: File
        private set
    private val lock = Any()
    private var day = ""
    private val dayFmt = SimpleDateFormat("yyyy-MM-dd", Locale.US)
    private val tsFmt = SimpleDateFormat("HH:mm:ss.SSS", Locale.US)

    private val _version = MutableStateFlow(0)
    val version: StateFlow<Int> = _version.asStateFlow()

    fun init(ctx: Context) {
        val dir = (ctx.getExternalFilesDir("logs") ?: File(ctx.filesDir, "logs")).also { it.mkdirs() }
        file = File(dir, "service.log")
        day = if (file.exists() && file.length() > 0) dayFmt.format(Date(file.lastModified())) else dayFmt.format(Date())
        i("LOG", "init, file=${file.absolutePath}")
    }

    fun i(tag: String, msg: String) = write("I", tag, msg)
    fun w(tag: String, msg: String) = write("W", tag, msg)
    fun e(tag: String, msg: String, t: Throwable? = null) =
        write("E", tag, if (t != null) "$msg: ${t.javaClass.simpleName} ${t.message}" else msg)

    private fun write(level: String, tag: String, msg: String) {
        if (!::file.isInitialized) return
        synchronized(lock) {
            try {
                val now = Date()
                val today = dayFmt.format(now)
                if (today != day) { // daily cleanup
                    file.writeText("")
                    day = today
                    file.appendText("${tsFmt.format(now)} I/LOG: new day $today, log cleared\n")
                }
                file.appendText("${tsFmt.format(now)} $level/$tag: $msg\n")
                if (file.length() > 5_000_000) { // safety cap
                    file.writeText(file.readLines().takeLast(5000).joinToString("\n", postfix = "\n"))
                }
            } catch (_: Exception) {
            }
        }
        _version.value++
    }

    fun readTail(maxLines: Int = 5000): List<String> = synchronized(lock) {
        if (!::file.isInitialized || !file.exists()) emptyList() else file.readLines().takeLast(maxLines)
    }

    fun clear() = synchronized(lock) {
        if (::file.isInitialized) file.writeText("")
        _version.value++
    }
}
