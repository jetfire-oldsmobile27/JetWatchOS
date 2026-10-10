package com.jetfire_oldsmobile.jetwatchapp.data

import android.content.Context

data class Settings(
    val address: String? = null,
    val goal: Int = 10_000,
    val syncIntervalMin: Int = 15,
    val mirrorEnabled: Boolean = true,
    val allowedPkgs: Set<String> = emptySet(),
    val brightness: Int = 128,
    val timeoutS: Int = 15,
    val cityName: String = "Moscow",
    val lat: Double = 55.75,
    val lon: Double = 37.62,
    val wifiSsid: String = "",
    val clearAfterImport: Boolean = false,
    val lastTrainingStart: Long = 0,
)

class SettingsStore(ctx: Context) {
    private val sp = ctx.getSharedPreferences("settings", Context.MODE_PRIVATE)

    fun load() = Settings(
        address = sp.getString("address", null),
        goal = sp.getInt("goal", 10_000),
        syncIntervalMin = sp.getInt("interval", 15),
        mirrorEnabled = sp.getBoolean("mirror", true),
        allowedPkgs = sp.getStringSet("pkgs", emptySet()) ?: emptySet(),
        brightness = sp.getInt("brightness", 128),
        timeoutS = sp.getInt("timeout", 15),
        cityName = sp.getString("city", "Moscow") ?: "Moscow",
        lat = sp.getFloat("lat", 55.75f).toDouble(),
        lon = sp.getFloat("lon", 37.62f).toDouble(),
        wifiSsid = sp.getString("ssid", "") ?: "",
        clearAfterImport = sp.getBoolean("clearAfter", false),
        lastTrainingStart = sp.getLong("lastTr", 0),
    )

    fun save(s: Settings) = sp.edit()
        .putString("address", s.address).putInt("goal", s.goal).putInt("interval", s.syncIntervalMin)
        .putBoolean("mirror", s.mirrorEnabled).putStringSet("pkgs", s.allowedPkgs.toSet())
        .putInt("brightness", s.brightness).putInt("timeout", s.timeoutS).putString("city", s.cityName)
        .putFloat("lat", s.lat.toFloat()).putFloat("lon", s.lon.toFloat()).putString("ssid", s.wifiSsid)
        .putBoolean("clearAfter", s.clearAfterImport).putLong("lastTr", s.lastTrainingStart)
        .apply()
}
