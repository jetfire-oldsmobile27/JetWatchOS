package com.jetfire_oldsmobile.jetwatchapp.proto

import org.json.JSONArray
import org.json.JSONObject
import java.util.UUID

/** BLE JSON protocol v1 (mirror of the firmware header). */
object Proto {
    val SERVICE: UUID = UUID.fromString("6e400001-b5a3-f393-e0a9-e50e24dcca9e")
    val RX: UUID = UUID.fromString("6e400002-b5a3-f393-e0a9-e50e24dcca9e") // phone -> watch (write)
    val TX: UUID = UUID.fromString("6e400003-b5a3-f393-e0a9-e50e24dcca9e") // watch -> phone (notify)
    val CCCD: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
    const val DEVICE_NAME = "TWatchS3"

    /** Short icon keys for the watch's built-in mini icons. */
    fun iconKey(pkg: String): String = when {
        pkg.startsWith("org.telegram") -> "tg"
        pkg == "com.whatsapp" || pkg == "com.whatsapp.w4b" -> "wa"
        pkg.startsWith("com.vkontakte") || pkg == "com.vk.im" -> "vk"
        pkg == "com.google.android.gm" || pkg.contains("mail") -> "mail"
        pkg.contains("messaging") || pkg.contains("mms") || pkg.contains("sms") -> "sms"
        pkg.contains("dialer") || pkg.contains("telecom") -> "call"
        pkg.startsWith("com.instagram") -> "ig"
        else -> "app"
    }
}

data class DayStepsItem(val ymd: String, val steps: Int)
data class AlarmItem(val idx: Int, val h: Int, val m: Int, val en: Boolean, val days: Int) {
    /** bit0 = Mon ... bit6 = Sun (0x1F = Mon-Fri). Confirm against firmware. */
    fun hasDay(d: Int) = days and (1 shl d) != 0
}
data class Training(val id: Int, val type: String, val start: Long, val dur: Long, val steps: Int, val dist: Int)

fun JSONObject.daysList(): List<DayStepsItem> = optJSONArray("days").mapObjects {
    DayStepsItem(it.optString("ymd"), it.optInt("steps"))
}

fun JSONObject.alarmsList(): List<AlarmItem> = optJSONArray("alarms").mapObjects {
    AlarmItem(it.optInt("idx"), it.optInt("h"), it.optInt("m"), it.optBoolean("en"), it.optInt("days"))
}

fun JSONObject.trainingList(): List<Training> = optJSONArray("items").mapObjects {
    Training(it.optInt("id"), it.optString("type"), it.optLong("start"), it.optLong("dur"), it.optInt("steps"), it.optInt("dist"))
}

private fun <T> JSONArray?.mapObjects(f: (JSONObject) -> T): List<T> {
    if (this == null) return emptyList()
    return (0 until length()).mapNotNull { optJSONObject(it)?.let(f) }
}
