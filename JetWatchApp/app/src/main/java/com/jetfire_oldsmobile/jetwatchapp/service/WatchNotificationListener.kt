package com.jetfire_oldsmobile.jetwatchapp.service

import android.app.Notification
import android.service.notification.NotificationListenerService
import android.service.notification.StatusBarNotification
import com.jetfire_oldsmobile.jetwatchapp.hub.WatchHub
import com.jetfire_oldsmobile.jetwatchapp.log.AppLog

class WatchNotificationListener : NotificationListenerService() {
    override fun onNotificationPosted(sbn: StatusBarNotification) {
        val n = sbn.notification
        val ex = n.extras
        val title = ex.getCharSequence(Notification.EXTRA_TITLE)?.toString().orEmpty()
        val body = (ex.getCharSequence(Notification.EXTRA_BIG_TEXT)
                ?: ex.getCharSequence(Notification.EXTRA_TEXT))?.toString().orEmpty()

        val isSummary = n.flags and Notification.FLAG_GROUP_SUMMARY != 0
        AppLog.i("NOTIF", "pkg=${sbn.packageName} summary=$isSummary " +
                "ongoing=${sbn.isOngoing} title='${title.take(30)}' body_len=${body.length}")

        if (sbn.packageName == packageName) return
        if (title.isBlank() && body.isBlank()) return
        if (isSummary && body.isBlank()) return

        if (sbn.packageName !in WatchHub.settings.value.allowedPkgs) {
            AppLog.i("NOTIF", "pkg=${sbn.packageName} not allowed")
            return
        }

        val label = runCatching {
            packageManager.getApplicationLabel(
                packageManager.getApplicationInfo(sbn.packageName, 0)
            ).toString()
        }.getOrDefault(sbn.packageName)

        WatchHub.mirror(sbn.packageName, label, title, body)
    }
}