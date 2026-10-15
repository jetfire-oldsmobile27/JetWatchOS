package com.jetfire_oldsmobile.jetwatchapp.service

import android.Manifest
import android.annotation.SuppressLint
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import androidx.core.app.NotificationChannelCompat
import androidx.core.app.NotificationCompat
import androidx.core.app.NotificationManagerCompat
import com.jetfire_oldsmobile.jetwatchapp.R

object Notif {
    const val CH_SERVICE = "service"
    const val CH_ALERT = "alerts"

    fun init(ctx: Context) {
        val nm = NotificationManagerCompat.from(ctx)
        // IMPORTANCE_MIN: FGS остаётся валидным, но иконки в статус-баре нет.
        nm.createNotificationChannel(
            NotificationChannelCompat.Builder(CH_SERVICE, NotificationManagerCompat.IMPORTANCE_MIN)
                .setName(ctx.getString(R.string.ch_service_name))
                .setDescription(ctx.getString(R.string.ch_service_desc))
                .setShowBadge(false)
                .build()
        )
        nm.createNotificationChannel(
            NotificationChannelCompat.Builder(CH_ALERT, NotificationManagerCompat.IMPORTANCE_DEFAULT)
                .setName(ctx.getString(R.string.ch_alerts_name))
                .build()
        )
    }

    @SuppressLint("MissingPermission")
    fun alert(ctx: Context, id: Int, title: String, text: String) {
        if (Build.VERSION.SDK_INT >= 33 &&
            ctx.checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED
        ) return
        NotificationManagerCompat.from(ctx).notify(
            id,
            NotificationCompat.Builder(ctx, CH_ALERT)
                .setSmallIcon(android.R.drawable.stat_sys_data_bluetooth)
                .setContentTitle(title).setContentText(text).setAutoCancel(true).build()
        )
    }
}