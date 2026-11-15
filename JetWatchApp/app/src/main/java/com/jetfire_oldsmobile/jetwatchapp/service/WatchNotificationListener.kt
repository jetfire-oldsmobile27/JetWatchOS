package com.jetfire_oldsmobile.jetwatchapp.service

import android.app.Notification
import android.service.notification.NotificationListenerService
import android.service.notification.StatusBarNotification
import com.jetfire_oldsmobile.jetwatchapp.hub.WatchHub
import com.jetfire_oldsmobile.jetwatchapp.log.AppLog

class WatchNotificationListener : NotificationListenerService() {

    /**
     * Кэш: sbn.key -> отпечаток последнего отправленного контента.
     * LinkedHashMap в режиме access-order + removeEldestEntry = простой LRU,
     * ограниченный 128 записями (хватит на все «живые» уведомления трея).
     */
    private val seen = object : LinkedHashMap<String, Int>(128, 0.75f, true) {
        override fun removeEldestEntry(
            eldest: MutableMap.MutableEntry<String, Int>?
        ): Boolean = size > 128
    }

    override fun onNotificationPosted(sbn: StatusBarNotification) {
        val n = sbn.notification
        val flags = n.flags

        // ── 1. Служебные / persistent уведомления не трогаем вообще ──
        if (sbn.isOngoing) {
            return                                        // музыка, загрузки, зарядка, BT, FGS
        }
        if (flags and Notification.FLAG_ONGOING_EVENT != 0) {
            return                                        // дублирует sbn.isOngoing, но надёжнее
        }
        if (flags and Notification.FLAG_FOREGROUND_SERVICE != 0) {
            return                                        // FGS-нотификации
        }

        // ── 2. Своё же приложение игнорируем ──
        if (sbn.packageName == packageName) return

        // ── 3. Извлекаем текст ──
        val ex = n.extras
        val title = ex.getCharSequence(Notification.EXTRA_TITLE)?.toString().orEmpty()
        val body  = (ex.getCharSequence(Notification.EXTRA_BIG_TEXT)
                ?: ex.getCharSequence(Notification.EXTRA_TEXT))?.toString().orEmpty()

        if (title.isBlank() && body.isBlank()) return

        val isSummary = flags and Notification.FLAG_GROUP_SUMMARY != 0
        // Summary без тела — это просто контейнер группы, зеркалить нечего.
        if (isSummary && body.isBlank()) return

        // ── 4. Дедуп по (sbn.key, хеш контента) ──
        val contentHash = 31 * title.hashCode() + body.hashCode()
        val prev = seen[sbn.key]
        if (prev != null && prev == contentHash) {
            AppLog.i("NOTIF", "skip re-post ${sbn.packageName} key=${sbn.key}")
            return
        }
        seen[sbn.key] = contentHash

        // ── 5. Пользовательский фильтр ──
        if (sbn.packageName !in WatchHub.settings.value.allowedPkgs) {
            AppLog.i("NOTIF", "pkg=${sbn.packageName} not allowed")
            return
        }

        // ── 6. Метка приложения ──
        val label = runCatching {
            packageManager.getApplicationLabel(
                packageManager.getApplicationInfo(sbn.packageName, 0)
            ).toString()
        }.getOrDefault(sbn.packageName)

        WatchHub.mirror(sbn.packageName, label, title, body)
    }

    override fun onNotificationRemoved(sbn: StatusBarNotification) {
        // Убираем запись — если пользователь смахнул нотификацию в трее,
        // а потом приложение её пересоздаст, хотим показать снова.
        seen.remove(sbn.key)
    }
}