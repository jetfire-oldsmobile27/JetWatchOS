package com.jetfire_oldsmobile.jetwatchapp.ble

import android.annotation.SuppressLint
import android.bluetooth.*
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.os.Build
import android.os.ParcelUuid
import com.jetfire_oldsmobile.jetwatchapp.hub.WatchHub
import com.jetfire_oldsmobile.jetwatchapp.log.AppLog
import com.jetfire_oldsmobile.jetwatchapp.proto.Proto
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withTimeoutOrNull
import org.json.JSONObject
import java.io.ByteArrayOutputStream
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.atomic.AtomicInteger

/**
 * GATT client for the watch. Line protocol: UTF-8 JSON + '\n', chunked by MTU-3.
 * Responses are matched to requests by "id"; objects with "ev" go to [events].
 */
@SuppressLint("MissingPermission")
class WatchBle(private val ctx: Context) {
    enum class Conn { Disconnected, Scanning, Connecting, Ready }

    private val _conn = MutableStateFlow(Conn.Disconnected)
    val conn = _conn.asStateFlow()
    val events = MutableSharedFlow<JSONObject>(extraBufferCapacity = 64)

    var deviceAddress: String? = null
        private set

    private val mgr = ctx.getSystemService(BluetoothManager::class.java)
    private var gatt: BluetoothGatt? = null
    private var rx: BluetoothGattCharacteristic? = null
    private var mtu = 23

    private var ready: CompletableDeferred<Boolean>? = null
    private var writeDone: CompletableDeferred<Boolean>? = null
    private val connectMutex = Mutex()
    private val writeMutex = Mutex()
    private val pending = ConcurrentHashMap<Int, CompletableDeferred<JSONObject>>()
    private val nextId = AtomicInteger(1)
    private val inBuf = ByteArrayOutputStream()

    
    private fun refreshGattCache(dev: BluetoothDevice) {
        runCatching {
            val m = dev.javaClass.getMethod("refresh")
            m.invoke(dev)
        }
    }

    /** address == null -> scan by name/service. */
    suspend fun connect(address: String?): Boolean = connectMutex.withLock {
        if (_conn.value == Conn.Ready) return@withLock true
        closeGatt()
        val adapter = mgr?.adapter
        if (adapter == null || !adapter.isEnabled) {
            AppLog.w("BLE", "Bluetooth adapter unavailable or off")
            return@withLock false
        }
        val dev = if (address != null) adapter.getRemoteDevice(address) else scan(adapter)
        if (dev == null) { _conn.value = Conn.Disconnected; return@withLock false }

        // важно: убрать stale gatt
        refreshGattCache(dev)
        delay(200)

        _conn.value = Conn.Connecting
        val r = CompletableDeferred<Boolean>()
        ready = r
        gatt = dev.connectGatt(ctx, false, callback, BluetoothDevice.TRANSPORT_LE)
        val ok = withTimeoutOrNull(15_000) { r.await() } ?: false
        if (ok) { deviceAddress = dev.address; AppLog.i("BLE", "ready, mtu=$mtu") }
        else    { AppLog.w("BLE", "connect failed/timeout"); closeGatt() }
        ok
    }

    fun disconnect() {
        AppLog.i("BLE", "disconnect requested")
        closeGatt()
    }

    private suspend fun scan(adapter: BluetoothAdapter): BluetoothDevice? {
        val scanner = adapter.bluetoothLeScanner ?: return null
        _conn.value = Conn.Scanning
        val found = CompletableDeferred<BluetoothDevice?>()
        val cb = object : ScanCallback() {
            override fun onScanResult(callbackType: Int, result: ScanResult) { found.complete(result.device) }
            override fun onScanFailed(errorCode: Int) { found.complete(null) }
        }
        val filters = listOf(
            ScanFilter.Builder().setDeviceName(Proto.DEVICE_NAME).build(),
            ScanFilter.Builder().setServiceUuid(ParcelUuid(Proto.SERVICE)).build(),
        )
        scanner.startScan(filters, ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build(), cb)
        val dev = withTimeoutOrNull(12_000) { found.await() }
        runCatching { scanner.stopScan(cb) }
        return dev
    }

    private fun closeGatt() {
        val g = gatt; gatt = null; rx = null
        runCatching { g?.disconnect() }
        runCatching { g?.close() }
        _conn.value = Conn.Disconnected
        ready?.complete(false)
        ready = null                       
        failPending()
    }

    private fun failPending() {
        val err = JSONObject().put("ok", false).put("err", "disconnected")
        pending.values.forEach { it.complete(err) }
        pending.clear()
        synchronized(inBuf) { inBuf.reset() }
    }

    /** Sends a command and waits for the reply with the same id. Returns null if not connected / timeout. */
    suspend fun request(cmd: String, extras: Map<String, Any?> = emptyMap(), timeoutMs: Long = 8_000): JSONObject? {
        if (_conn.value != Conn.Ready) return null
        val id = nextId.getAndIncrement()
        val o = JSONObject().put("id", id).put("cmd", cmd)
        extras.forEach { (k, v) -> if (v != null) o.put(k, v) }
        val d = CompletableDeferred<JSONObject>()
        pending[id] = d
        val line = o.toString()
        AppLog.i("TX", line.replace(Regex("\"pwd\":\"[^\"]*\""), "\"pwd\":\"***\""))
        if (!send(line)) {
            pending.remove(id)
            AppLog.w("BLE", "write failed for $cmd")
            return null
        }
        val r = withTimeoutOrNull(timeoutMs) { d.await() }
        pending.remove(id)
        if (r == null) AppLog.w("BLE", "timeout waiting reply to $cmd")
        return r
    }

    private suspend fun send(line: String): Boolean = writeMutex.withLock {
        val g = gatt ?: return@withLock false
        val c = rx ?: return@withLock false
        val bytes = (line + "\n").toByteArray(Charsets.UTF_8)
        val chunk = (mtu - 3).coerceAtLeast(20)
        var off = 0
        while (off < bytes.size) {
            val part = bytes.copyOfRange(off, minOf(off + chunk, bytes.size))
            val d = CompletableDeferred<Boolean>()
            writeDone = d
            if (!writeChar(g, c, part)) return@withLock false
            if (withTimeoutOrNull(3_000) { d.await() } != true) return@withLock false
            off += part.size
        }
        true
    }

    @Suppress("DEPRECATION")
    private fun writeChar(g: BluetoothGatt, c: BluetoothGattCharacteristic, data: ByteArray): Boolean =
        if (Build.VERSION.SDK_INT >= 33) {
            g.writeCharacteristic(c, data, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT) == BluetoothStatusCodes.SUCCESS
        } else {
            c.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
            c.value = data
            g.writeCharacteristic(c)
        }

    private fun onData(bytes: ByteArray) {
        synchronized(inBuf) {
            for (b in bytes) {
                if (b == '\n'.code.toByte()) {
                    val line = inBuf.toString(Charsets.UTF_8.name()).trim()
                    inBuf.reset()
                    if (line.isNotEmpty()) handleLine(line)
                } else inBuf.write(b.toInt())
            }
        }
    }

    private fun handleLine(line: String) {
        AppLog.i("RX", line)
        val o = try { JSONObject(line) } catch (e: Exception) {
            AppLog.w("BLE", "bad json: ${e.message}"); return
        }
        if (o.has("ev")) { events.tryEmit(o); return }
        val id = o.optInt("id", -1)
        pending.remove(id)?.complete(o)
    }

    private val callback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) {
            AppLog.i("BLE", "state=$newState status=$status")
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                if (!g.requestMtu(247)) g.discoverServices()
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                if (g === gatt) { closeGatt() } else runCatching { g.close() }
                // автопереподключение через 3 c, если сервис ещё работает
                if (WatchHub.state.value.serviceRunning) {
                    WatchHub.scope.launch {
                        delay(3000)
                        if (WatchHub.state.value.serviceRunning) WatchHub.connectNow()
                    }
                }
            }
        }

        override fun onMtuChanged(g: BluetoothGatt, mtu: Int, status: Int) {
            this@WatchBle.mtu = mtu
            g.discoverServices()
        }

        override fun onServicesDiscovered(g: BluetoothGatt, status: Int) {
            val svc = g.getService(Proto.SERVICE)
            val tx = svc?.getCharacteristic(Proto.TX)
            rx = svc?.getCharacteristic(Proto.RX)
            if (tx == null || rx == null) {
                AppLog.e("BLE", "service/characteristics not found")
                ready?.complete(false); return
            }
            g.setCharacteristicNotification(tx, true)
            val cccd = tx.getDescriptor(Proto.CCCD)
            if (cccd == null) { ready?.complete(false); return }
            writeDescriptor(g, cccd)
        }

        override fun onDescriptorWrite(g: BluetoothGatt, d: BluetoothGattDescriptor, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                _conn.value = Conn.Ready
                ready?.complete(true)
            } else ready?.complete(false)
        }

        override fun onCharacteristicWrite(g: BluetoothGatt, c: BluetoothGattCharacteristic, status: Int) {
            writeDone?.complete(status == BluetoothGatt.GATT_SUCCESS)
        }

        // API 33+
        override fun onCharacteristicChanged(g: BluetoothGatt, c: BluetoothGattCharacteristic, value: ByteArray) = onData(value)

        // API < 33
        @Suppress("DEPRECATION", "OVERRIDE_DEPRECATION")
        override fun onCharacteristicChanged(g: BluetoothGatt, c: BluetoothGattCharacteristic) {
            if (Build.VERSION.SDK_INT < 33) c.value?.let(::onData)
        }
    }

    @Suppress("DEPRECATION")
    private fun writeDescriptor(g: BluetoothGatt, d: BluetoothGattDescriptor) {
        if (Build.VERSION.SDK_INT >= 33) {
            g.writeDescriptor(d, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE)
        } else {
            d.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
            g.writeDescriptor(d)
        }
    }
}
