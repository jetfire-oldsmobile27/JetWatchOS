package com.jetfire_oldsmobile.jetwatchapp.ui

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import androidx.core.app.NotificationManagerCompat

fun blePerms(): Array<String> =
    if (Build.VERSION.SDK_INT >= 31) arrayOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)
    else arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)

fun hasBle(c: Context) = blePerms().all { c.checkSelfPermission(it) == PackageManager.PERMISSION_GRANTED }

fun hasPostNotif(c: Context) =
    Build.VERSION.SDK_INT < 33 || c.checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) == PackageManager.PERMISSION_GRANTED

fun hasListener(c: Context) = NotificationManagerCompat.getEnabledListenerPackages(c).contains(c.packageName)
