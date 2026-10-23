package com.jetfire_oldsmobile.jetwatchapp.ui

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.jetfire_oldsmobile.jetwatchapp.hub.WatchHub
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asSharedFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch

class WatchViewModel(app: Application) : AndroidViewModel(app) {
    val state = WatchHub.state
    val settings = WatchHub.settings
    private val _msg = MutableSharedFlow<String>(extraBufferCapacity = 8)
    val msg = _msg.asSharedFlow()

    private val _busy = MutableStateFlow<String?>(null)
    val busy = _busy.asStateFlow()

    fun cmd(label: String, name: String, extras: Map<String, Any?> = emptyMap(), onOk: () -> Unit = {}) {
        viewModelScope.launch {
            val r = WatchHub.command(name, extras)
            val ok = r?.optBoolean("ok") == true
            if (ok) onOk()
            _msg.emit(if (ok) "$label: OK" else "$label: failed")
        }
    }

    fun sync() = viewModelScope.launch {
        if (_busy.value != null) return@launch
        _busy.value = "sync"
        try {
            if (!WatchHub.connectNow()) _msg.emit("Watch not reachable")
        } finally { _busy.value = null }
    }

    fun connect(scanNew: Boolean) = viewModelScope.launch {
        if (_busy.value != null) return@launch
        _busy.value = if (scanNew) "scan" else "connect"
        try {
            if (!WatchHub.connectNow(scanNew)) _msg.emit("Watch not found")
        } finally { _busy.value = null }
    }

    fun disconnect() = WatchHub.ble.disconnect()
    fun forget() = WatchHub.forget()
    fun refreshHealth() = viewModelScope.launch { WatchHub.refreshHealth() }
    fun update(block: (com.jetfire_oldsmobile.jetwatchapp.data.Settings) -> com.jetfire_oldsmobile.jetwatchapp.data.Settings) =
        WatchHub.updateSettings(block)
}
