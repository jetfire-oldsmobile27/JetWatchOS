package com.jetfire_oldsmobile.jetwatchapp

import android.app.Application
import com.jetfire_oldsmobile.jetwatchapp.hub.WatchHub
import com.jetfire_oldsmobile.jetwatchapp.log.AppLog
import com.jetfire_oldsmobile.jetwatchapp.service.Notif

class WatchApp : Application() {
    override fun onCreate() {
        super.onCreate()
        AppLog.init(this)
        Notif.init(this)
        WatchHub.init(this)
    }
}
