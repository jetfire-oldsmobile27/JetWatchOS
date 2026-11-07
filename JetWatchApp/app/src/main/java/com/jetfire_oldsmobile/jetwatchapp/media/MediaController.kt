package com.jetfire_oldsmobile.jetwatchapp.media

import android.content.Context
import android.media.AudioManager
import android.view.KeyEvent
import android.media.session.MediaSessionManager
import android.media.session.PlaybackState
import com.jetfire_oldsmobile.jetwatchapp.log.AppLog

class MediaController(private val ctx: Context) {

    private val audio = ctx.getSystemService(AudioManager::class.java)
    private val sessionMgr = ctx.getSystemService(MediaSessionManager::class.java)

    /** Отправить медиа-клавишу в активную сессию. */
    fun sendKey(keyCode: Int) {
        // 1) Пробуем через активную сессию (даёт play/pause, next, prev)
        val session = activeSession()
        if (session != null && keyCode in setOf(
                KeyEvent.KEYCODE_MEDIA_NEXT,
                KeyEvent.KEYCODE_MEDIA_PREVIOUS,
                KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE
            )) {
            session.controller.transportControls.let { tc ->
                when (keyCode) {
                    KeyEvent.KEYCODE_MEDIA_NEXT -> tc.skipToNext()
                    KeyEvent.KEYCODE_MEDIA_PREVIOUS -> tc.skipToPrevious()
                    KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE -> {
                        val state = session.controller.playbackState?.state
                        if (state == PlaybackState.STATE_PLAYING) tc.pause()
                        else tc.play()
                    }
                }
            }
            AppLog.i("MEDIA", "session cmd key=$keyCode")
            return
        }

        // 2) Fallback: глобальный диспетчер (громкость, а также next/prev,
        //    если сессия не отдаёт transport controls)
        val down = KeyEvent(KeyEvent.ACTION_DOWN, keyCode)
        val up = KeyEvent(KeyEvent.ACTION_UP, keyCode)
        audio.dispatchMediaKeyEvent(down)
        audio.dispatchMediaKeyEvent(up)
        AppLog.i("MEDIA", "dispatch key=$keyCode")
    }

    private fun activeSession(): MediaSessionManager.RemoteUserInfo? {
        val user = android.os.Process.myUserHandle()
        return runCatching { sessionMgr.getActiveSessions(null).firstOrNull() }
            .getOrNull()
            ?.let { /* RemoteUserInfo не нужен, нужен контроллер */ }
            ?.let { null } // заглушка: ниже используем другой путь
    }
}