package com.jetfire_oldsmobile.jetwatchapp.service

import android.app.Notification
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.os.IBinder
import androidx.core.app.NotificationCompat
import androidx.core.app.ServiceCompat
import androidx.core.content.ContextCompat
import com.jetfire_oldsmobile.jetwatchapp.MainActivity
import com.jetfire_oldsmobile.jetwatchapp.ble.WatchBle
import com.jetfire_oldsmobile.jetwatchapp.hub.WatchHub
import com.jetfire_oldsmobile.jetwatchapp.hub.WatchState
import com.jetfire_oldsmobile.jetwatchapp.log.AppLog
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.launch

/** Foreground service keeping the process alive; the periodic work itself lives in [WatchHub.startLoop]. */
class WatchService : Service() {
    private var obs: Job? = null

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (intent?.action == ACTION_STOP) {
            stopSelf(); return START_NOT_STICKY
        }
        ServiceCompat.startForeground(
            this, 1, build(WatchHub.state.value), ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE
        )
        AppLog.i("SERVICE", "started")
        WatchHub.stopLoop()
        WatchHub.startLoop()
        obs?.cancel()
        obs = WatchHub.scope.launch {
            WatchHub.state.map { Triple(it.conn, it.battery, it.todaySteps) }.distinctUntilChanged().collect {
                getSystemService(NotificationManager::class.java).notify(1, build(WatchHub.state.value))
            }
        }
        return START_STICKY
    }

    override fun onDestroy() {
        AppLog.i("SERVICE", "stopped")
        obs?.cancel()
        WatchHub.stopLoop()
        super.onDestroy()
    }

    private fun build(s: WatchState): Notification {
        val status = when (s.conn) {
            WatchBle.Conn.Ready -> "Connected" + (s.battery?.let { " · $it%" } ?: "") + " · ${s.todaySteps} steps"
            WatchBle.Conn.Scanning -> "Searching for watch…"
            WatchBle.Conn.Connecting -> "Connecting…"
            WatchBle.Conn.Disconnected -> "Disconnected, retrying"
        }
        val open = PendingIntent.getActivity(
            this, 0, Intent(this, MainActivity::class.java), PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT
        )
        val stop = PendingIntent.getService(
            this, 1, Intent(this, WatchService::class.java).setAction(ACTION_STOP), PendingIntent.FLAG_IMMUTABLE
        )
        return NotificationCompat.Builder(this, Notif.CH_SERVICE)
            .setSmallIcon(android.R.drawable.stat_sys_data_bluetooth)
            .setContentTitle("JetWatch").setContentText(status)
            .setContentIntent(open).setOngoing(true).setOnlyAlertOnce(true)
            .addAction(0, "Stop", stop).build()
    }

    companion object {
        private const val ACTION_STOP = "stop"
        fun start(ctx: Context) = ContextCompat.startForegroundService(ctx, Intent(ctx, WatchService::class.java))
        fun stop(ctx: Context) { ctx.stopService(Intent(ctx, WatchService::class.java)) }
    }
}
